#include "mgr_relay.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "int_homekit.h"
#include "svc_espnow.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "mgr_relay";

typedef struct {
    int relay_gpio;
    int touch_gpio;
    bool state;
    button_handle_t btn_handle;
} relay_ctx_t;

static relay_ctx_t s_relays[2];
static mgr_relay_bridge_cb_t s_bridge_cb = NULL;

static void relay_toggle_cb(void *button_handle, void *usr_data)
{
    int idx = (int)(uint32_t)usr_data;
    bool new_state = !s_relays[idx].state;
    ESP_LOGI(TAG, "Button %d pressed, toggling to %d", idx, new_state);
    mgr_relay_set_state(idx, new_state, true);
}

#if defined(CONFIG_APP_ESPNOW_RELAY_SLAVE) || defined(CONFIG_APP_ESPNOW_RELAY_MASTER)
static uint8_t s_remote_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool parse_mac_address(const char *str, uint8_t *mac) {
  if (!str || strlen(str) != 17) return false;
  int values[6];
  if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x", &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) == 6) {
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)values[i];
    return true;
  }
  return false;
}
#endif

#ifdef CONFIG_APP_ESPNOW_RELAY_SLAVE
static void relay_espnow_handler(uint8_t relay_idx, bool state)
{
    if (relay_idx < 2) {
        ESP_LOGI(TAG, "ESP-NOW received: Relay %d -> %d", relay_idx, state);
        mgr_relay_set_state(relay_idx, state, true);
    }
}
#endif

esp_err_t mgr_relay_init(void)
{
#ifdef CONFIG_APP_ESPNOW_RELAY_DISABLED
    return ESP_OK;
#endif

    ESP_LOGI(TAG, "Initializing Touch Relay module");

    // Initialize GPIOs and Buttons if hardware is enabled
#ifdef CONFIG_APP_RELAY_HARDWARE_ENABLE
    int relay_gpios[2] = {CONFIG_APP_RELAY1_GPIO, CONFIG_APP_RELAY2_GPIO};
    int touch_gpios[2] = {CONFIG_APP_TOUCH1_GPIO, CONFIG_APP_TOUCH2_GPIO};

    for (int i = 0; i < 2; i++) {
        s_relays[i].relay_gpio = relay_gpios[i];
        s_relays[i].touch_gpio = touch_gpios[i];
        
        // Restore state from NVS
        uint8_t saved_state = 0;
        nvs_handle_t nvs_h;
        if (nvs_open("storage", NVS_READONLY, &nvs_h) == ESP_OK) {
            char nvs_key[16];
            snprintf(nvs_key, sizeof(nvs_key), "relay_st_%d", i);
            nvs_get_u8(nvs_h, nvs_key, &saved_state);
            nvs_close(nvs_h);
        }
        s_relays[i].state = saved_state;

        // Init Relay GPIO
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << s_relays[i].relay_gpio),
            .pull_down_en = 0,
            .pull_up_en = 0,
        };
        gpio_config(&io_conf);
        gpio_set_level(s_relays[i].relay_gpio, s_relays[i].state);

        // Init Button
        button_config_t btn_cfg = {
            .long_press_time = 1000,
            .short_press_time = 50,
        };
        button_gpio_config_t gpio_cfg = {
            .gpio_num = s_relays[i].touch_gpio,
            .active_level = 0,
        };
        iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &s_relays[i].btn_handle);
        if (s_relays[i].btn_handle) {
            iot_button_register_cb(s_relays[i].btn_handle, BUTTON_SINGLE_CLICK, NULL, relay_toggle_cb, (void*)(uint32_t)i);
        }
    }
#endif

#ifdef CONFIG_APP_ESPNOW_RELAY_MASTER
    parse_mac_address(CONFIG_APP_ESPNOW_RELAY_MAC, s_remote_mac);
#endif

    // Register ESP-NOW handler if in Slave mode
#ifdef CONFIG_APP_ESPNOW_RELAY_SLAVE
    svc_espnow_register_relay_handler(relay_espnow_handler);
#endif

    return ESP_OK;
}

void mgr_relay_set_state(int idx, bool state, bool report_to_homekit)
{
    if (idx < 0 || idx >= 2) return;

    s_relays[idx].state = state;

#ifdef CONFIG_APP_RELAY_HARDWARE_ENABLE
    gpio_set_level(s_relays[idx].relay_gpio, state);
    
    // Save to NVS
    nvs_handle_t nvs_h;
    if (nvs_open("storage", NVS_READWRITE, &nvs_h) == ESP_OK) {
        char nvs_key[16];
        snprintf(nvs_key, sizeof(nvs_key), "relay_st_%d", idx);
        nvs_set_u8(nvs_h, nvs_key, (uint8_t)state);
        nvs_commit(nvs_h);
        nvs_close(nvs_h);
    }
#endif

    // Logic for Master/Slave/Standalone
    if (s_bridge_cb) {
        s_bridge_cb((uint8_t)idx, state);
    }

    if (report_to_homekit) {
        int_homekit_update_relay(idx, state);
    }
}

bool mgr_relay_get_state(int idx)
{
    if (idx < 0 || idx >= 2) return false;
    return s_relays[idx].state;
}

void mgr_relay_set_bridge_cb(mgr_relay_bridge_cb_t cb)
{
    s_bridge_cb = cb;
}
