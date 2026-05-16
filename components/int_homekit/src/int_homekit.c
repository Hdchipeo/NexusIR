#include "int_homekit.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "sdkconfig.h"
#include <math.h>
#include <string.h>

// Only compile if iOS Platform is selected
#if CONFIG_LAMP_PLATFORM_IOS

#include "app_hap_setup_payload.h"
#include "hap.h"
#include "hap_apple_chars.h"
#include "hap_apple_servs.h"
#include "mgr_ac_logic.h"
#include "mgr_fan_logic.h"
#include "svc_web_server.h"

// Optional Sensor Support
#if CONFIG_LAMP_SENSOR_AHT20
#include "drv_aht20.h"
#endif

#include "drv_led.h"
#include "mgr_relay.h"

static const char *TAG = "int_homekit";

static hap_serv_t *s_thermostat_service = NULL;
static hap_char_t *s_curr_temp_char = NULL;
static hap_char_t *s_target_temp_char = NULL;
static hap_char_t *s_curr_state_char = NULL;
static hap_char_t *s_target_state_char = NULL;

// Optional Sensor Services
#if CONFIG_LAMP_SENSOR_AHT20
static hap_serv_t *s_temp_sensor_service = NULL;
static hap_serv_t *s_hum_sensor_service = NULL;
static hap_char_t *s_sensor_curr_temp_char = NULL;
static hap_char_t *s_sensor_curr_hum_char = NULL;
#endif

// Web UI Switch
static hap_serv_t *s_web_switch_service = NULL;
static hap_char_t *s_web_switch_on_char = NULL;

// LED Services
#define MAX_LEDS 5
static hap_serv_t *s_led_service[MAX_LEDS] = {NULL};
static hap_char_t *s_led_on_char[MAX_LEDS] = {NULL};
static hap_char_t *s_led_brightness_char[MAX_LEDS] = {NULL};
static hap_char_t *s_led_hue_char[MAX_LEDS] = {NULL};
static hap_char_t *s_led_sat_char[MAX_LEDS] = {NULL};

// FAN Services
static hap_serv_t *s_fan_service = NULL;
static hap_char_t *s_fan_on_char = NULL;
static hap_char_t *s_fan_speed_char = NULL;
static hap_char_t *s_fan_direction_char = NULL;

// Relay Services
static hap_serv_t *s_relay_service[2] = {NULL};
static hap_char_t *s_relay_on_char[2] = {NULL};

static float s_led_h[MAX_LEDS] = {0.0f};
static float s_led_s[MAX_LEDS] = {0.0f};
static int s_led_v[MAX_LEDS] = {100};
static bool s_led_on[MAX_LEDS] = {false};

static void helper_hsv2rgb(float h, float s, float v, uint8_t *r, uint8_t *g,
                           uint8_t *b) {
  s /= 100.0f;
  v /= 100.0f;
  int i = (int)(h / 60.0f);
  float f = (h / 60.0f) - i;
  float p = v * (1.0f - s);
  float q = v * (1.0f - s * f);
  float t = v * (1.0f - s * (1.0f - f));
  float rf = 0, gf = 0, bf = 0;
  switch (i % 6) {
  case 0:
    rf = v, gf = t, bf = p;
    break;
  case 1:
    rf = q, gf = v, bf = p;
    break;
  case 2:
    rf = p, gf = v, bf = t;
    break;
  case 3:
    rf = p, gf = q, bf = v;
    break;
  case 4:
    rf = t, gf = p, bf = v;
    break;
  case 5:
    rf = v, gf = p, bf = q;
    break;
  }
  *r = (uint8_t)(rf * 255);
  *g = (uint8_t)(gf * 255);
  *b = (uint8_t)(bf * 255);
}

static int led_write(hap_write_data_t write_data[], int count, void *serv_priv,
                     void *write_priv) {
  uint8_t lamp_id = (uint8_t)(uint32_t)serv_priv;
  if (lamp_id >= MAX_LEDS) return HAP_FAIL;
  
  bool update_color = false;
  for (int i = 0; i < count; i++) {
    hap_write_data_t *data = &write_data[i];
    const char *uuid = hap_char_get_type_uuid(data->hc);

    if (strcmp(uuid, HAP_CHAR_UUID_ON) == 0) {
      s_led_on[lamp_id] = data->val.b;
      drv_led_set_power(lamp_id, s_led_on[lamp_id] ? 1 : 0);
      if (s_led_on[lamp_id]) {
        drv_led_set_effect(lamp_id, DRV_LED_EFFECT_STATIC);
        drv_led_set_state(DRV_LED_IDLE);
      }
    } else if (strcmp(uuid, HAP_CHAR_UUID_BRIGHTNESS) == 0) {
      s_led_v[lamp_id] = data->val.i;
      drv_led_set_brightness(lamp_id, s_led_v[lamp_id]);
      update_color = true;
    } else if (strcmp(uuid, HAP_CHAR_UUID_HUE) == 0) {
      s_led_h[lamp_id] = data->val.f;
      update_color = true;
    } else if (strcmp(uuid, HAP_CHAR_UUID_SATURATION) == 0) {
      s_led_s[lamp_id] = data->val.f;
      update_color = true;
    }

    *(data->status) = HAP_STATUS_SUCCESS;
    hap_char_update_val(data->hc, &(data->val));
  }

  if (update_color) {
    uint8_t r, g, b;
    helper_hsv2rgb(s_led_h[lamp_id], s_led_s[lamp_id], 100.0f, &r, &g, &b);
    drv_led_set_color(lamp_id, r, g, b);
    if (s_led_on[lamp_id]) {
      drv_led_set_effect(lamp_id, DRV_LED_EFFECT_STATIC);
    }
  }

  return HAP_SUCCESS;
}
// End of led_write

/* HAP Main Task */
static void hap_task(void *p) {
  // HAP manages its own tasks internally, just keep this task alive
  // and update sensor data if enabled
  uint32_t health_counter = 0;
  while (1) {
#if CONFIG_LAMP_SENSOR_AHT20
    float temp = 0.0f;
    float hum = 0.0f;
    if (aht20_sensor_read(&temp, &hum) == ESP_OK) {
      // Update Temperature Sensor Service
      if (s_sensor_curr_temp_char) {
        hap_val_t val = {.f = temp};
        hap_char_update_val(s_sensor_curr_temp_char, &val);
      }
      // Update Humidity Sensor Service
      if (s_sensor_curr_hum_char) {
        hap_val_t val = {.f = hum};
        hap_char_update_val(s_sensor_curr_hum_char, &val);
      }
      // Also update Thermostat Current Temperature if desired
      if (s_curr_temp_char) {
        hap_val_t val = {.f = temp};
        hap_char_update_val(s_curr_temp_char, &val);
      }
    }
#endif

    // Periodic health check (every 60s = 12 loops × 5s)
    // Helps diagnose stale sessions and memory leaks
    health_counter++;
    if (health_counter >= 12) {
      health_counter = 0;
      int paired = hap_get_paired_controller_count();
      ESP_LOGI(TAG, "[Health] Paired controllers: %d | Free heap: %lu | Min heap: %lu",
               paired, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    }

    vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds
  }
  vTaskDelete(NULL);
}

/* Mandatory Identify callback */
static int hap_identify(hap_acc_t *ha) {
  ESP_LOGI(TAG, "Accessory Identification Requested");
  return HAP_SUCCESS;
}

/*
 * Callback for HomeKit write operations
 */
static int thermostat_write(hap_write_data_t write_data[], int count,
                            void *serv_priv, void *write_priv) {
  ESP_LOGI(TAG, "HomeKit Write Request: %d items", count);

  ir_ac_state_t state;
  mgr_ac_get_state(&state);
  bool changed = false;

  for (int i = 0; i < count; i++) {
    hap_write_data_t *data = &write_data[i];
    const char *uuid = hap_char_get_type_uuid(data->hc);

    ESP_LOGI(TAG, "Writing Char: %s", uuid);

    if (strcmp(uuid, HAP_CHAR_UUID_TARGET_HEATING_COOLING_STATE) == 0) {
      int val = data->val.i;
      ESP_LOGI(TAG, "Target State: %d", val);

      // Map HAP State (0:Off, 1:Heat, 2:Cool, 3:Auto) to AC State
      switch (val) {
      case 0: // Off
        state.power = false;
        break;
      case 1: // Heat
        state.power = true;
        state.mode = 2; // Heat
        break;
      case 2: // Cool
        state.power = true;
        state.mode = 1; // Cool
        break;
      case 3: // Auto
        state.power = true;
        state.mode = 0; // Auto
        break;
      }
      changed = true;

    } else if (strcmp(uuid, HAP_CHAR_UUID_TARGET_TEMPERATURE) == 0) {
      float val = data->val.f;
      ESP_LOGI(TAG, "Target Temp: %.1f", val);
      state.temp = (uint8_t)val;
      changed = true;
    }

    hap_char_update_val(data->hc, &(data->val));
    *(data->status) = HAP_STATUS_SUCCESS;
  }

  if (changed) {
    mgr_ac_set_state(&state);
    mgr_ac_send(); // Transmit IR
  }

  return HAP_SUCCESS;
}

static int fan_write(hap_write_data_t write_data[], int count,
                     void *serv_priv, void *write_priv) {
  ESP_LOGI(TAG, "HomeKit Fan Write Request: %d items", count);

  ir_fan_state_t state;
  mgr_fan_get_state(&state);
  bool changed = false;

  for (int i = 0; i < count; i++) {
    hap_write_data_t *data = &write_data[i];
    const char *uuid = hap_char_get_type_uuid(data->hc);

    if (strcmp(uuid, HAP_CHAR_UUID_ACTIVE) == 0 || strcmp(uuid, HAP_CHAR_UUID_ON) == 0) {
      bool power = (data->val.i == 1 || data->val.b == true);
      if (state.power != power) {
          state.power = power;
          changed = true;
      }
    } else if (strcmp(uuid, HAP_CHAR_UUID_ROTATION_SPEED) == 0) {
      float val = data->val.f;
      uint8_t new_speed = 1;
      if (val > 0) {
        state.power = true;
        if (val <= 33.33) new_speed = 1;
        else if (val <= 66.66) new_speed = 2;
        else new_speed = 3;
      } else {
        state.power = false;
      }
      
      if (state.speed != new_speed) {
          state.speed = new_speed;
          changed = true;
      }
    } else if (strcmp(uuid, HAP_CHAR_UUID_SWING_MODE) == 0) {
      bool swing = (data->val.i == 1);
      if (state.swing != swing) {
          state.swing = swing;
          changed = true;
      }
    } else if (strcmp(uuid, HAP_CHAR_UUID_ROTATION_DIRECTION) == 0) {
        // Just update value to avoid error, logic can be added later
        changed = false; 
    } else {
        ESP_LOGW(TAG, "Unknown Fan Characteristic UUID: %s", uuid);
    }

    hap_char_update_val(data->hc, &(data->val));
    *(data->status) = HAP_STATUS_SUCCESS;
  }

  if (changed) {
    ESP_LOGI(TAG, "Fan state changed via HomeKit: P=%d, S=%d, Sw=%d. Triggering IR...", 
             state.power, state.speed, state.swing);
    mgr_fan_set_state(&state);
    mgr_fan_send();
  }

  return HAP_SUCCESS;
}

static int relay_write(hap_write_data_t write_data[], int count,
                       void *serv_priv, void *write_priv) {
  int idx = (int)(uint32_t)serv_priv;
  for (int i = 0; i < count; i++) {
    hap_write_data_t *data = &write_data[i];
    if (strcmp(hap_char_get_type_uuid(data->hc), HAP_CHAR_UUID_ON) == 0) {
      bool on = data->val.b;
      ESP_LOGI(TAG, "HomeKit: Relay %d -> %s", idx, on ? "ON" : "OFF");
      mgr_relay_set_state(idx, on, false); // Don't report back to HomeKit to avoid loop
      hap_char_update_val(data->hc, &(data->val));
      *(data->status) = HAP_STATUS_SUCCESS;
    }
  }
  return HAP_SUCCESS;
}

static int webui_write(hap_write_data_t write_data[], int count,
                       void *serv_priv, void *write_priv) {
  for (int i = 0; i < count; i++) {
    hap_write_data_t *data = &write_data[i];
    if (strcmp(hap_char_get_type_uuid(data->hc), HAP_CHAR_UUID_ON) == 0) {
      bool enable = data->val.b;
      ESP_LOGI(TAG, "HomeKit: Web UI Switch %s", enable ? "ON" : "OFF");
      if (enable) {
        svc_web_start();
      } else {
        svc_web_stop();
      }
      hap_char_update_val(data->hc, &(data->val));
      *(data->status) = HAP_STATUS_SUCCESS;
    }
  }
  return HAP_SUCCESS;
}

/* Update HomeKit state from device state (Poll/Push) */
void int_homekit_update_state(const ir_ac_state_t *state) {
  if (!s_thermostat_service)
    return;

  hap_val_t val;

  // Current State (HAP mapping)
  int curr_state = 0;
  if (state->power) {
    if (state->mode == 2)
      curr_state = 2; // Heating
    else if (state->mode == 1)
      curr_state = 3; // Cooling
    else
      curr_state = 2; // Default active
  }

  val.i = curr_state;
  hap_char_update_val(s_curr_state_char, &val);

  // Target State
  int target_state = 0; // Off
  if (state->power) {
    if (state->mode == 2)
      target_state = 1; // Heat
    else if (state->mode == 1)
      target_state = 2; // Cool
    else
      target_state = 3; // Auto
  }
  val.i = target_state;
  hap_char_update_val(s_target_state_char, &val);

  // Temps
  val.f = (float)state->temp;
  hap_char_update_val(s_curr_temp_char, &val);
  hap_char_update_val(s_target_temp_char, &val);
}

void int_homekit_update_led(uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t speed) {
  if (lamp_id >= MAX_LEDS || !s_led_service[lamp_id]) return;

  hap_val_t val;

  val.b = (power > 0);
  hap_char_update_val(s_led_on_char[lamp_id], &val);

  val.i = brightness;
  hap_char_update_val(s_led_brightness_char[lamp_id], &val);

  float h = 0, s = 0;
  float fr = r / 255.0f, fg = g / 255.0f, fb = b / 255.0f;
  float max = fmaxf(fr, fmaxf(fg, fb)), min = fminf(fr, fminf(fg, fb));
  float d = max - min;
  s = max == 0 ? 0 : d / max;
  if (max != min) {
    if (max == fr) h = (fg - fb) / d + (fg < fb ? 6 : 0);
    else if (max == fg) h = (fb - fr) / d + 2;
    else h = (fr - fg) / d + 4;
    h /= 6;
  }

  val.f = h * 360.0f;
  hap_char_update_val(s_led_hue_char[lamp_id], &val);
  val.f = s * 100.0f;
  hap_char_update_val(s_led_sat_char[lamp_id], &val);
}

void int_homekit_update_fan_state(const ir_fan_state_t *state) {
  if (!s_fan_service)
    return;

  hap_val_t val;
  val.b = state->power;
  hap_char_update_val(s_fan_on_char, &val);

  val.f = state->speed == 1 ? 33.33f : (state->speed == 2 ? 66.66f : 100.0f);
  hap_char_update_val(s_fan_speed_char, &val);

  hap_char_t *swing_char = hap_serv_get_char_by_uuid(s_fan_service, HAP_CHAR_UUID_SWING_MODE);
  if (swing_char) {
      val.i = state->swing ? 1 : 0;
      hap_char_update_val(swing_char, &val);
  }
}

void int_homekit_update_temp(float temperature, float humidity) {
  // Update Temperature Sensor Service
#if CONFIG_LAMP_SENSOR_AHT20
  if (s_sensor_curr_temp_char) {
    hap_val_t val = {.f = temperature};
    hap_char_update_val(s_sensor_curr_temp_char, &val);
  }
  // Update Humidity Sensor Service
  if (s_sensor_curr_hum_char) {
    hap_val_t val = {.f = humidity};
    hap_char_update_val(s_sensor_curr_hum_char, &val);
  }
#endif
  // Also update Thermostat Current Temperature if desired
  if (s_curr_temp_char) {
    hap_val_t val = {.f = temperature};
    hap_char_update_val(s_curr_temp_char, &val);
  }
}

void int_homekit_update_relay(uint8_t relay_idx, bool state) {
  if (relay_idx < 2 && s_relay_on_char[relay_idx]) {
    hap_val_t val = {.b = state};
    hap_char_update_val(s_relay_on_char[relay_idx], &val);
  }
}

static bool s_is_inited = false;

// ... (existing code)

bool int_homekit_is_paired(void) {
  if (!s_is_inited) {
    hap_init(HAP_TRANSPORT_WIFI);
    s_is_inited = true;
  }
  return (hap_get_paired_controller_count() > 0);
}

esp_err_t int_homekit_init(void) {
  ESP_LOGI(TAG, "Initializing HomeKit (Bridge Mode)...");

  // 1. Initialize HAP (if not already done via is_paired check)
  if (!s_is_inited) {
    // Suppress verbose HAP logs (Value Changed, Notification Sent, etc.)
    hap_set_debug_level(HAP_DEBUG_LEVEL_WARN);

    hap_init(HAP_TRANSPORT_WIFI);
    s_is_inited = true;
  }

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char serial[16];
  snprintf(serial, sizeof(serial), "NX-%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);

  // -------------------------------------------------------------------------
  // 2. Create BRIDGE Accessory (Primary)
  // -------------------------------------------------------------------------
  hap_acc_cfg_t bridge_cfg = {
      .name = "NexusIR Bridge",
      .manufacturer = "NexusIR Inc",
      .model = "LP-BRIDGE-01",
      .serial_num = serial,
      .fw_rev = "1.3.0",
      .hw_rev = "1.0",
      .pv = "1.1.0",
      .identify_routine = hap_identify,
      .cid = HAP_CID_BRIDGE,
  };
  hap_acc_t *bridge_acc = hap_acc_create(&bridge_cfg);
  if (!bridge_acc) {
    ESP_LOGE(TAG, "Failed to create Bridge accessory");
    return ESP_FAIL;
  }

  // Add Product Data & Transport to Bridge
  uint8_t product_data[] = {'E', 'S', 'P', '3', '2', 'H', 'K', 'T'};
  hap_acc_add_product_data(bridge_acc, product_data, sizeof(product_data));
  hap_acc_add_wifi_transport_service(bridge_acc, 0);

  // Add Bridge to HAP Database (Primary) - CRITICAL!
  hap_add_accessory(bridge_acc);

  // -------------------------------------------------------------------------
  // 3. Create AC Accessory (Bridged)
#ifndef CONFIG_APP_ESPNOW_AC_DISABLED
#endif
  // -------------------------------------------------------------------------
  char ac_serial[20];
  snprintf(ac_serial, sizeof(ac_serial), "%s-AC", serial);
  hap_acc_cfg_t ac_cfg = {
      .name = "NexusIR AC",
      .manufacturer = "NexusIR Inc",
      .model = "LP-IR-01",
      .serial_num = ac_serial,
      .fw_rev = "1.3.0",
      .hw_rev = "1.0",
      .pv = "1.1.0",
      .identify_routine = hap_identify,
      .cid = HAP_CID_THERMOSTAT,
  };
  hap_acc_t *ac_acc = hap_acc_create(&ac_cfg);
  if (ac_acc) {
    hap_acc_add_product_data(ac_acc, product_data, sizeof(product_data));

    // Create Thermostat Service
    s_thermostat_service = hap_serv_thermostat_create(0, 0, 24.0, 24.0, 0);

    // Get handles
    s_curr_temp_char = hap_serv_get_char_by_uuid(
        s_thermostat_service, HAP_CHAR_UUID_CURRENT_TEMPERATURE);
    s_target_temp_char = hap_serv_get_char_by_uuid(
        s_thermostat_service, HAP_CHAR_UUID_TARGET_TEMPERATURE);
    s_curr_state_char = hap_serv_get_char_by_uuid(
        s_thermostat_service, HAP_CHAR_UUID_CURRENT_HEATING_COOLING_STATE);
    s_target_state_char = hap_serv_get_char_by_uuid(
        s_thermostat_service, HAP_CHAR_UUID_TARGET_HEATING_COOLING_STATE);

    // Set Bounds & Callback
    hap_char_float_set_constraints(s_target_temp_char, 16.0, 32.0, 1.0);
    hap_serv_set_write_cb(s_thermostat_service, thermostat_write);
    hap_acc_add_serv(ac_acc, s_thermostat_service);

// Optional Sensors
#if CONFIG_LAMP_SENSOR_AHT20
    s_temp_sensor_service = hap_serv_temperature_sensor_create(0.0f);
    if (s_temp_sensor_service) {
      s_sensor_curr_temp_char = hap_serv_get_char_by_uuid(
          s_temp_sensor_service, HAP_CHAR_UUID_CURRENT_TEMPERATURE);
      hap_acc_add_serv(ac_acc, s_temp_sensor_service);
    }
    s_hum_sensor_service = hap_serv_humidity_sensor_create(0.0f);
    if (s_hum_sensor_service) {
      s_sensor_curr_hum_char = hap_serv_get_char_by_uuid(
          s_hum_sensor_service, HAP_CHAR_UUID_CURRENT_RELATIVE_HUMIDITY);
      hap_acc_add_serv(ac_acc, s_hum_sensor_service);
    }
#endif

    // Add AC as Bridged Accessory
    hap_add_bridged_accessory(ac_acc, hap_get_unique_aid("NexusIR AC"));
    ESP_LOGI(TAG, "Added Bridged AC Accessory");
  }

  // -------------------------------------------------------------------------
  // Create LED Lightbulb Accessories (Bridged)
  // -------------------------------------------------------------------------
  const char *led_names[MAX_LEDS] = {
#ifdef CONFIG_APP_LED1_NAME
      CONFIG_APP_LED1_NAME,
#else
      "Lamp 1",
#endif
#ifdef CONFIG_APP_LED2_NAME
      CONFIG_APP_LED2_NAME,
#else
      "Lamp 2",
#endif
#ifdef CONFIG_APP_LED3_NAME
      CONFIG_APP_LED3_NAME,
#else
      "Lamp 3",
#endif
#ifdef CONFIG_APP_LED4_NAME
      CONFIG_APP_LED4_NAME,
#else
      "Lamp 4",
#endif
#ifdef CONFIG_APP_LED5_NAME
      CONFIG_APP_LED5_NAME
#else
      "Lamp 5"
#endif
  };
  
  bool led_enabled[MAX_LEDS] = {
#ifndef CONFIG_APP_ESPNOW_LED1_DISABLED
      true,
#else
      false,
#endif
#ifndef CONFIG_APP_ESPNOW_LED2_DISABLED
      true,
#else
      false,
#endif
#ifndef CONFIG_APP_ESPNOW_LED3_DISABLED
      true,
#else
      false,
#endif
#ifndef CONFIG_APP_ESPNOW_LED4_DISABLED
      true,
#else
      false,
#endif
#ifndef CONFIG_APP_ESPNOW_LED5_DISABLED
      true
#else
      false
#endif
  };

  for (int i=0; i<MAX_LEDS; i++) {
    if (!led_enabled[i]) continue;
    
    char serial[32]; snprintf(serial, sizeof(serial), "LED-%02d", i);
    hap_acc_cfg_t led_cfg = {
        .name = (char*)led_names[i],
        .manufacturer = "NexusIR Inc",
        .model = "LP-LED-01",
        .serial_num = serial,
        .fw_rev = "1.0.0",
        .hw_rev = "1.0",
        .pv = "1.1.0",
        .identify_routine = hap_identify,
        .cid = HAP_CID_LIGHTING,
    };
    hap_acc_t *led_acc = hap_acc_create(&led_cfg);
    if (led_acc) {
      hap_acc_add_product_data(led_acc, product_data, sizeof(product_data));

      s_led_service[i] = hap_serv_lightbulb_create(false);
      if (s_led_service[i]) {
        hap_serv_set_priv(s_led_service[i], (void*)(uint32_t)i);
        hap_serv_add_char(s_led_service[i], hap_char_name_create((char*)led_names[i]));

        s_led_brightness_char[i] = hap_char_brightness_create(100);
        hap_serv_add_char(s_led_service[i], s_led_brightness_char[i]);

        s_led_hue_char[i] = hap_char_hue_create(0);
        hap_serv_add_char(s_led_service[i], s_led_hue_char[i]);

        s_led_sat_char[i] = hap_char_saturation_create(0);
        hap_serv_add_char(s_led_service[i], s_led_sat_char[i]);

        s_led_on_char[i] = hap_serv_get_char_by_uuid(s_led_service[i], HAP_CHAR_UUID_ON);

        hap_serv_set_write_cb(s_led_service[i], led_write);
        hap_acc_add_serv(led_acc, s_led_service[i]);

        hap_add_bridged_accessory(led_acc, hap_get_unique_aid((char*)led_names[i]));
        ESP_LOGI(TAG, "Added Bridged LED Accessory: %s", led_names[i]);
      }
    }
  }

  // -------------------------------------------------------------------------
  // 4. Create Web Switch Accessory (Bridged)
  // -------------------------------------------------------------------------
  hap_acc_cfg_t web_cfg = {
      .name = "Web Config",
      .manufacturer = "NexusIR Inc",
      .model = "LP-WEB-01",
      .serial_num = "001122334499",
      .fw_rev = "1.0.0",
      .hw_rev = "1.0",
      .pv = "1.1.0",
      .identify_routine = hap_identify,
      .cid = HAP_CID_SWITCH, // Category Switch
  };
  hap_acc_t *web_acc = hap_acc_create(&web_cfg);
  if (web_acc) {
    hap_acc_add_product_data(web_acc, product_data, sizeof(product_data));

    s_web_switch_service = hap_serv_switch_create(false); // Default OFF
    if (s_web_switch_service) {
      hap_serv_add_char(s_web_switch_service,
                        hap_char_name_create("Web Config Mode"));
      s_web_switch_on_char =
          hap_serv_get_char_by_uuid(s_web_switch_service, HAP_CHAR_UUID_ON);
      hap_serv_set_write_cb(s_web_switch_service, webui_write);

      // Add Service to WEB Accessory
      hap_acc_add_serv(web_acc, s_web_switch_service);

      hap_add_bridged_accessory(web_acc, hap_get_unique_aid("Web Config"));
      ESP_LOGI(TAG, "Added Bridged Web Switch Accessory");
    }
  }

  // -------------------------------------------------------------------------
  // Create Fan Accessory (Bridged)
  // -------------------------------------------------------------------------
  hap_acc_cfg_t fan_cfg = {
      .name = "NexusIR Fan",
      .manufacturer = "NexusIR Inc",
      .model = "LP-FAN-01",
      .serial_num = "001122334466",
      .fw_rev = "1.0.0",
      .hw_rev = "1.0",
      .pv = "1.1.0",
      .identify_routine = hap_identify,
      .cid = HAP_CID_FAN,
  };
  hap_acc_t *fan_acc = hap_acc_create(&fan_cfg);
  if (fan_acc) {
    hap_acc_add_product_data(fan_acc, product_data, sizeof(product_data));

    s_fan_service = hap_serv_fan_v2_create(false);
    if (s_fan_service) {
      s_fan_on_char = hap_serv_get_char_by_uuid(s_fan_service, HAP_CHAR_UUID_ON);
      
      // Add Rotation Speed
      s_fan_speed_char = hap_char_rotation_speed_create(0);
      hap_serv_add_char(s_fan_service, s_fan_speed_char);

      // Add Swing Mode
      hap_char_t *swing_char = hap_char_swing_mode_create(0);
      hap_serv_add_char(s_fan_service, swing_char);

      hap_serv_set_write_cb(s_fan_service, fan_write);
      hap_acc_add_serv(fan_acc, s_fan_service);

      hap_add_bridged_accessory(fan_acc, hap_get_unique_aid("NexusIR Fan"));
      ESP_LOGI(TAG, "Added Bridged Fan Accessory");
    }
  }

  // -------------------------------------------------------------------------
  // Create Relay Accessories (Bridged)
  // -------------------------------------------------------------------------
#ifndef CONFIG_APP_ESPNOW_RELAY_DISABLED
  const char *relay_names[2] = {CONFIG_APP_RELAY1_NAME, CONFIG_APP_RELAY2_NAME};
  for (int i = 0; i < 2; i++) {
    char r_serial[32]; snprintf(r_serial, sizeof(r_serial), "RELAY-%02X%02X-%d", mac[4], mac[5], i);
    hap_acc_cfg_t r_cfg = {
        .name = (char*)relay_names[i],
        .manufacturer = "NexusIR Inc",
        .model = "LP-RELAY-01",
        .serial_num = r_serial,
        .fw_rev = "1.0.0",
        .hw_rev = "1.0",
        .pv = "1.1.0",
        .identify_routine = hap_identify,
        .cid = HAP_CID_LIGHTING,
    };
    hap_acc_t *r_acc = hap_acc_create(&r_cfg);
    if (r_acc) {
      hap_acc_add_product_data(r_acc, product_data, sizeof(product_data));
      s_relay_service[i] = hap_serv_lightbulb_create(mgr_relay_get_state(i));
      if (s_relay_service[i]) {
        hap_serv_set_priv(s_relay_service[i], (void*)(uint32_t)i);
        s_relay_on_char[i] = hap_serv_get_char_by_uuid(s_relay_service[i], HAP_CHAR_UUID_ON);
        hap_serv_set_write_cb(s_relay_service[i], relay_write);
        hap_acc_add_serv(r_acc, s_relay_service[i]);
        hap_add_bridged_accessory(r_acc, hap_get_unique_aid((char*)relay_names[i]));
        ESP_LOGI(TAG, "Added Bridged Relay Accessory: %s", relay_names[i]);
      }
    }
  }
#endif

  hap_set_setup_code("111-22-333");
  hap_set_setup_id("LP4C");

  // Print Setup Instructions
  ESP_LOGI(TAG, "================================================");
  ESP_LOGI(TAG, "  HomeKit Pairing Setup (Bridge Mode)");
  ESP_LOGI(TAG, "  Setup Code: 111-22-333");
  ESP_LOGI(TAG, "  Setup ID: LP4C");
  ESP_LOGI(TAG, "================================================");

  // QR Code for Bridge
  app_hap_setup_payload("111-22-333", "LP4C", false, HAP_CID_BRIDGE);

  // ✅ CRITICAL: Restore state from AC module BEFORE hap_start()
  // This ensures HomeKit displays correct state after reboot
  ESP_LOGI(TAG, "Restoring HomeKit characteristics from AC state...");

  ir_ac_state_t current_state;
  mgr_ac_get_custom_brand(); // Wait, app_ac_get_state(&current_state);
  mgr_ac_get_state(&current_state);

  // Restore Temperature characteristics
  if (s_curr_temp_char && s_target_temp_char) {
    hap_val_t temp_val = {.f = (float)current_state.temp};
    hap_char_update_val(s_target_temp_char, &temp_val);
    ESP_LOGI(TAG, "Restored target temperature: %.0f°C", temp_val.f);
  }

  // Restore State characteristics (Map AC mode to HomeKit states)
  if (s_curr_state_char && s_target_state_char) {
    int hk_state = 0; // 0=OFF, 1=HEAT, 2=COOL, 3=AUTO

    if (current_state.power) {
      if (current_state.mode == 2) {
        hk_state = 1; // Heat
      } else if (current_state.mode == 1) {
        hk_state = 2; // Cool
      } else {
        hk_state = 3; // Auto or other modes
      }
    } else {
      hk_state = 0; // OFF
    }

    hap_val_t state_val = {.i = hk_state};
    hap_char_update_val(s_curr_state_char, &state_val);
    hap_char_update_val(s_target_state_char, &state_val);

    const char *state_names[] = {"OFF", "HEAT", "COOL", "AUTO"};
    ESP_LOGI(TAG, "Restored thermostat state: %s (AC power=%d, mode=%d)",
             state_names[hk_state], current_state.power, current_state.mode);
  }

  // Restore Fan characteristics
  if (s_fan_service) {
    ir_fan_state_t fan_state;
    mgr_fan_get_state(&fan_state);
    int_homekit_update_fan_state(&fan_state);
    ESP_LOGI(TAG, "Restored fan state: Power=%d, Speed=%d, Swing=%d", 
             fan_state.power, fan_state.speed, fan_state.swing);
  }

  ESP_LOGI(TAG, "HomeKit state restoration complete");

  hap_enable_mfi_auth(HAP_MFI_AUTH_NONE);
  hap_start();
  xTaskCreate(hap_task, "hap_task", 8192, NULL, 1, NULL);
  ESP_LOGI(TAG, "HomeKit Bridge Started.");

  return ESP_OK;
}

#else

esp_err_t int_homekit_init(void) {
  return ESP_OK; // Dummy
}

#endif
