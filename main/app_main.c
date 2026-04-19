#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include "drv_aht20.h"
#include "drv_button.h"
#include "drv_led.h"
#include "int_homekit.h" // Assuming we will migrate this next or stub it
#include "int_rainmaker.h"
#include "mgr_ac_logic.h"
#include "mgr_fan_logic.h"
#include "mgr_ir_protocols.h"
#include "svc_espnow.h"
#include "svc_log.h"
#include "svc_mdns.h"
#include "svc_nvs.h"
#include "svc_ota.h"
#include "svc_web_server.h"
#include "svc_wifi.h"
#include "sys_mem.h"

// RainMaker headers
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_params.h>

#define TAG "main"

// Suppress verbose RainMaker param logs (sensor reporting spam)
__attribute__((constructor)) static void suppress_rmaker_logs(void) {
  esp_log_level_set("esp_rmaker_param", ESP_LOG_WARN);
}
#if CONFIG_LAMP_PLATFORM_ANDROID
static void web_ui_toggle_cb(bool enable) {
  if (enable) {
    svc_web_start();
  } else {
    svc_web_stop();
  }
}
#endif

#if CONFIG_LAMP_PLATFORM_IOS

static void ios_homekit_init_task(void *arg) {
  bool is_paired = int_homekit_is_paired();
  if (!is_paired) {
    // If Not Paired: Init mDNS FIRST
    ESP_LOGI(TAG, "HomeKit NOT Paired: Starting mDNS before HomeKit");
    svc_mdns_init();
    int_homekit_init();
  } else {
    // If Paired: Init HomeKit FIRST
    ESP_LOGI(TAG, "HomeKit Paired: Starting HomeKit before mDNS");
    int_homekit_init();

    // Delayed mDNS initialization to dodge spinlock collisions
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Delayed mDNS initialization...");
    svc_mdns_init();
  }
  vTaskDelete(NULL);
}

// Event handler for iOS to start HomeKit after WiFi connection
static void ios_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(TAG, "WiFi connected, launching HomeKit init task");
    xTaskCreate(ios_homekit_init_task, "hk_init", 8192, NULL, 5, NULL);
  }
}
#endif

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    svc_ota_mark_valid();
#if !CONFIG_LAMP_PLATFORM_IOS
    // Android/Generic: Start mDNS here (No HomeKit conflict)
    svc_mdns_init();
#endif
  }
}

#if CONFIG_APP_ESPNOW_BRIDGE_ENABLE || CONFIG_APP_ESPNOW_SLAVE_ENABLE
static bool s_espnow_rx_in_progress = false;
#endif

// --- Master (Bridge) Adaptors ---
#if CONFIG_APP_ESPNOW_AC_MASTER
static void ac_bridge_send_adaptor(const ir_ac_state_t *state, ac_brand_t brand,
                                   const char *custom_name) {
  if (s_espnow_rx_in_progress)
    return;
  svc_espnow_bridge_ac_send(state, brand, custom_name ? custom_name : "");
}
#endif

#if CONFIG_APP_ESPNOW_LED1_MASTER || CONFIG_APP_ESPNOW_LED2_MASTER || CONFIG_APP_ESPNOW_LED3_MASTER || CONFIG_APP_ESPNOW_LED4_MASTER || CONFIG_APP_ESPNOW_LED5_MASTER
#define HAS_LED_MASTER 1
#endif

#if CONFIG_APP_ESPNOW_LED1_SLAVE || CONFIG_APP_ESPNOW_LED2_SLAVE || CONFIG_APP_ESPNOW_LED3_SLAVE || CONFIG_APP_ESPNOW_LED4_SLAVE || CONFIG_APP_ESPNOW_LED5_SLAVE
#define HAS_LED_SLAVE 1
#endif

#ifdef HAS_LED_MASTER
static void led_bridge_send_adaptor(uint8_t lamp_id, uint8_t power, uint8_t effect,
                                    uint8_t brightness, uint8_t r, uint8_t g,
                                    uint8_t b, uint8_t speed) {
  if (s_espnow_rx_in_progress)
    return;
  svc_espnow_bridge_led_send(lamp_id, power, effect, brightness, r, g, b, speed);
}
#endif

#if CONFIG_APP_ESPNOW_FAN_MASTER
static void fan_bridge_send_adaptor(const ir_fan_state_t *state, fan_brand_t brand,
                                    const char *custom_name) {
  if (s_espnow_rx_in_progress)
    return;
  svc_espnow_bridge_fan_send(state, brand, custom_name ? custom_name : "");
}
#endif

// --- Slave (Receiver) Adaptors ---
#if CONFIG_APP_ESPNOW_AC_SLAVE
static void ac_espnow_handler_adaptor(const ir_ac_state_t *state,
                                      ac_brand_t brand,
                                      const char *custom_name) {
  s_espnow_rx_in_progress = true;
  // Only sync state (power/mode/temp/fan), keep local brand
  // Slave has its own learned IR keys under its local brand
  mgr_ac_set_state(state);
  mgr_ac_send();
  int_homekit_update_state(state);
  s_espnow_rx_in_progress = false;
}
#endif

#ifdef HAS_LED_SLAVE
static void led_espnow_handler_adaptor(uint8_t lamp_id, uint8_t power, uint8_t effect,
                                       uint8_t brightness, uint8_t r, uint8_t g,
                                       uint8_t b, uint8_t speed) {
  s_espnow_rx_in_progress = true;
  drv_led_set_brightness(lamp_id, brightness);
  drv_led_set_speed(lamp_id, speed);
  drv_led_set_color(lamp_id, r, g, b);
  drv_led_set_power(lamp_id, power);
  if (power)
    drv_led_set_effect(lamp_id, (drv_led_effect_t)effect);
  int_homekit_update_led(lamp_id, power, effect, brightness, r, g, b, speed);
  s_espnow_rx_in_progress = false;
}
#endif

#if CONFIG_APP_ESPNOW_FAN_SLAVE
static void fan_espnow_handler_adaptor(const ir_fan_state_t *state,
                                       fan_brand_t brand,
                                       const char *custom_name) {
  s_espnow_rx_in_progress = true;
  // Only sync state (power/speed/swing), keep local brand
  // Slave has its own learned IR keys under its local brand
  mgr_fan_set_state(state);
  mgr_fan_send();
  int_homekit_update_fan_state(state);
  s_espnow_rx_in_progress = false;
}
#endif

// --- Temperature ESP-NOW ---
#if CONFIG_APP_ESPNOW_TEMP_SLAVE
static void temp_report_task(void *arg) {
  // Wait for ESP-NOW to be fully initialized
  vTaskDelay(pdMS_TO_TICKS(5000));
  ESP_LOGI(TAG, "Temp report task started (30s interval)");
  while (1) {
    float t = 0, h = 0;
    if (aht20_sensor_read(&t, &h) == ESP_OK) {
      svc_espnow_bridge_temp_send(t, h);
    }
    vTaskDelay(pdMS_TO_TICKS(30000)); // Report every 30 seconds
  }
}
#endif

#if CONFIG_APP_ESPNOW_TEMP_MASTER
static void temp_espnow_handler(float temp, float humidity) {
  ESP_LOGI(TAG, "Temp from Slave: %.1f°C, %.1f%%", temp, humidity);
  int_homekit_update_temp(temp, humidity);
}
#endif

void app_main(void) {
  svc_log_init();
  sys_mem_init();
  ESP_LOGI(TAG, "Starting Lamp IR Device...");
  ESP_LOGI(TAG, "Project version: %s", PROJECT_VERSION);

  // 1. Initialize NVS (Non-Volatile Storage)
  ESP_ERROR_CHECK(svc_nvs_init());

  // 2. Initialize Network Stack
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &ip_event_handler, NULL));

  // 3. Initialize IR Remote (Priority for RMT resources)
  if (mgr_ir_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init IR");
  }

  // 4. Initialize Peripherals (LED, Button)
  ESP_ERROR_CHECK(drv_led_init());
  drv_led_set_state(DRV_LED_STARTUP);

  ESP_ERROR_CHECK(drv_button_init());

  // 5. Initialize AC Logic (Load state from NVS)
#ifndef CONFIG_APP_ESPNOW_AC_DISABLED
  mgr_ac_init();
#endif
#ifndef CONFIG_APP_ESPNOW_FAN_DISABLED
  mgr_fan_init();
#endif

  // 6. Initialize Wi-Fi Stack
  svc_wifi_init();
  svc_espnow_init();

  // Register per-device ESP-NOW Master (Bridge) callbacks
#if CONFIG_APP_ESPNOW_AC_MASTER
  mgr_ac_set_bridge_cb(ac_bridge_send_adaptor);
#endif
#ifdef HAS_LED_MASTER
  drv_led_set_bridge_cb(led_bridge_send_adaptor);
#endif
#if CONFIG_APP_ESPNOW_FAN_MASTER
  mgr_fan_set_bridge_cb(fan_bridge_send_adaptor);
#endif
#if CONFIG_APP_ESPNOW_BRIDGE_ENABLE
  mgr_ir_start_slave(); // IR background RX needed if any device is Master
#endif

  // Register per-device ESP-NOW Slave (Receiver) handlers
#if CONFIG_APP_ESPNOW_AC_SLAVE
  svc_espnow_register_ac_handler(ac_espnow_handler_adaptor);
#endif
#ifdef HAS_LED_SLAVE
  svc_espnow_register_led_handler(led_espnow_handler_adaptor);
#endif
#if CONFIG_APP_ESPNOW_FAN_SLAVE
  svc_espnow_register_fan_handler(fan_espnow_handler_adaptor);
#endif
#if CONFIG_APP_ESPNOW_TEMP_MASTER
  svc_espnow_register_temp_handler(temp_espnow_handler);
#endif

  // 7. Initialize AHT20 Sensor (Optional, not needed in Master mode)
#if CONFIG_LAMP_SENSOR_AHT20
  if (drv_aht20_init() == ESP_OK) {
    ESP_LOGI(TAG, "AHT20 Sensor Initialized");
#if CONFIG_APP_ESPNOW_TEMP_SLAVE
    xTaskCreate(temp_report_task, "temp_rpt", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "Temp ESP-NOW Slave: reporting task started");
#endif
  } else {
    ESP_LOGW(TAG, "AHT20 Sensor Init Failed");
  }
#endif

  // 8. Initialize Mobile Platform Logic
#if CONFIG_LAMP_PLATFORM_ANDROID
  // [Android] Initialize RainMaker
  int_rainmaker_register_webui_toggle(web_ui_toggle_cb);
  int_rainmaker_init();
#elif CONFIG_LAMP_PLATFORM_IOS
  // [iOS] Register event handler to init HomeKit after WiFi connected
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &ios_wifi_event_handler, NULL));
  ESP_LOGI(TAG, "iOS mode: HomeKit will start after WiFi connection");
#endif

  // 9. Start Provisioning / Network
  svc_wifi_start(NULL);

  // 10. mDNS will be initialized automatically after WiFi connects (in
  // svc_wifi.c event handler)

  // 11. Initialize Web Server resources
  // On iOS, we init but don't start it automatically (avoid conflict).
  if (svc_web_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init Web Server resources");
  }
#if CONFIG_LAMP_PLATFORM_IOS
  ESP_LOGI(TAG,
           "iOS mode: Web server initialized but stopped (Toggle via HomeKit)");
#endif

  // 12. Initialize Automatic OTA
  // Periodically checks for firmware updates from CONFIG_OTA_SERVER_URL.
  svc_ota_auto_init();

  ESP_LOGI(TAG, "Initialization Complete. Device ready.");

  // Main Loop - Keep task alive (or can be deleted)
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
