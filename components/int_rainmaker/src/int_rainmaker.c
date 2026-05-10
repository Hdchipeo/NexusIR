#include "int_rainmaker.h"
#include "esp_log.h"
#include "esp_rmaker_core.h"
#include "esp_rmaker_standard_devices.h"
#include "esp_rmaker_standard_params.h"
#include "esp_rmaker_standard_types.h"
#include "esp_rmaker_common_events.h"
#include "sdkconfig.h"
#include "ir_types.h"
#include "mgr_ac_logic.h"
#include "mgr_fan_logic.h"
#include "mgr_ir_protocols.h"
#include "drv_led.h"
#include "mgr_relay.h"
#include "svc_nvs.h"
#include "svc_ota.h"
#include "cJSON.h"
#include <ctype.h>
#include <string.h>
#include <math.h>

#if CONFIG_LAMP_PLATFORM_ANDROID

static const char *TAG = "nexus_rainmaker";
#define PARAM_CUSTOM_BRAND_NAME "Custom Brand Name"
#define PARAM_WEBUI_MODE "WebUI Config Mode"

// Global Device Handles
esp_rmaker_device_t *ac_device = NULL;
esp_rmaker_device_t *fan_device = NULL;
esp_rmaker_device_t *led_devices[5] = {NULL};
esp_rmaker_device_t *relay_devices[2] = {NULL};
static webui_toggle_cb_t s_webui_cb = NULL;

static char **g_brand_str_list = NULL;
static size_t g_brand_list_count = 0;

void app_rainmaker_register_webui_toggle(webui_toggle_cb_t cb) {
  s_webui_cb = cb;
}

// --- Internal Helpers ---

static void rmaker_auto_turn_off_task(void *pv) {
  const esp_rmaker_param_t *param = (const esp_rmaker_param_t *)pv;
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_rmaker_param_update_and_report(param, esp_rmaker_bool(false));
  vTaskDelete(NULL);
}

// --- Callbacks ---

static esp_err_t ac_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                             const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx) {
    const char *param_name = esp_rmaker_param_get_name(param);
    ir_ac_state_t state;
    mgr_ac_get_state(&state);

    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) state.power = val.val.b;
    else if (strcmp(param_name, ESP_RMAKER_DEF_TEMPERATURE_NAME) == 0) state.temp = (int)val.val.f;
    else if (strcmp(param_name, "Mode") == 0) {
        if (strcmp(val.val.s, "Auto") == 0) state.mode = 0;
        else if (strcmp(val.val.s, "Cool") == 0) state.mode = 1;
        else if (strcmp(val.val.s, "Heat") == 0) state.mode = 2;
        else if (strcmp(val.val.s, "Fan") == 0) state.mode = 3;
        else if (strcmp(val.val.s, "Dry") == 0) state.mode = 4;
    } else if (strcmp(param_name, "Fan Speed") == 0) {
        if (strcmp(val.val.s, "Auto") == 0) state.fan = 0;
        else if (strcmp(val.val.s, "Low") == 0) state.fan = 1;
        else if (strcmp(val.val.s, "Medium") == 0) state.fan = 2;
        else if (strcmp(val.val.s, "High") == 0) state.fan = 3;
    } else if (strcmp(param_name, "Brand") == 0) {
        mgr_ac_set_custom_brand(val.val.s);
    }

    mgr_ac_set_state(&state);
    mgr_ac_send();
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static esp_err_t fan_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                              const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx) {
    const char *param_name = esp_rmaker_param_get_name(param);
    ir_fan_state_t state;
    mgr_fan_get_state(&state);

    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) state.power = val.val.b;
    else if (strcmp(param_name, "Speed") == 0) {
        if (strcmp(val.val.s, "Auto") == 0) state.speed = 0;
        else if (strcmp(val.val.s, "Low") == 0) state.speed = 1;
        else if (strcmp(val.val.s, "Medium") == 0) state.speed = 2;
        else if (strcmp(val.val.s, "High") == 0) state.speed = 3;
    }

    mgr_fan_set_state(&state);
    mgr_fan_send();
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static esp_err_t led_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                              const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx) {
    uint8_t lamp_id = (uint8_t)(uintptr_t)priv_data;
    const char *param_name = esp_rmaker_param_get_name(param);

    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) drv_led_set_power(lamp_id, val.val.b);
    else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) drv_led_set_brightness(lamp_id, val.val.i);
    
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static esp_err_t relay_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx) {
    uint8_t idx = (uint8_t)(uintptr_t)priv_data;
    mgr_relay_set_state(idx, val.val.b, true);
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static esp_err_t custom_btn_rmaker_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx) {
    const char *key_str = (const char *)priv_data;
    if (val.val.b && key_str) {
        ESP_LOGI(TAG, "Rainmaker Custom Button Triggered: %s", key_str);
        mgr_ir_send_key(key_str);
        xTaskCreate(rmaker_auto_turn_off_task, "rmk_auto_off", 2048, (void*)param, 5, NULL);
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static esp_err_t tv_rmaker_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                    const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx) {
    const char *short_brand = (const char *)priv_data;
    if (val.val.b && short_brand) {
        char key_str[32];
        snprintf(key_str, sizeof(key_str), "T_%s_PWR", short_brand);
        ESP_LOGI(TAG, "Rainmaker TV Triggered: %s", key_str);
        mgr_ir_send_key(key_str);
        xTaskCreate(rmaker_auto_turn_off_task, "rmk_auto_off", 2048, (void*)param, 5, NULL);
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static esp_err_t common_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                 const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx) {
    const char *param_name = esp_rmaker_param_get_name(param);
    if (strcmp(param_name, PARAM_WEBUI_MODE) == 0) {
        if (s_webui_cb) s_webui_cb(val.val.b);
        esp_rmaker_param_update_and_report(param, val);
    }
    return ESP_OK;
}

// --- State Sync Functions ---

void app_rainmaker_update_ac(const ir_ac_state_t *state) {
    if (!ac_device || !state) return;
    esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(ac_device, ESP_RMAKER_DEF_POWER_NAME), esp_rmaker_bool(state->power));
    esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(ac_device, ESP_RMAKER_DEF_TEMPERATURE_NAME), esp_rmaker_float((float)state->temp));
    static const char *modes[] = {"Auto", "Cool", "Heat", "Fan", "Dry"};
    if (state->mode < 5) esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(ac_device, "Mode"), esp_rmaker_str(modes[state->mode]));
    static const char *fans[] = {"Auto", "Low", "Medium", "High"};
    if (state->fan < 4) esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(ac_device, "Fan Speed"), esp_rmaker_str(fans[state->fan]));
    const char *brand = mgr_ac_get_custom_brand();
    if (brand) esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(ac_device, "Brand"), esp_rmaker_str(brand));
}

void app_rainmaker_update_fan(const ir_fan_state_t *state) {
    if (!fan_device || !state) return;
    esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(fan_device, ESP_RMAKER_DEF_POWER_NAME), esp_rmaker_bool(state->power));
    static const char *speeds[] = {"Auto", "Low", "Medium", "High"};
    if (state->speed < 4) esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(fan_device, "Speed"), esp_rmaker_str(speeds[state->speed]));
}

void app_rainmaker_update_led(uint8_t lamp_id, uint8_t power, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b) {
    if (lamp_id >= 5 || !led_devices[lamp_id]) return;
    esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(led_devices[lamp_id], ESP_RMAKER_DEF_POWER_NAME), esp_rmaker_bool(power));
    esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(led_devices[lamp_id], ESP_RMAKER_DEF_BRIGHTNESS_NAME), esp_rmaker_int(brightness));
}

void app_rainmaker_update_relay(uint8_t idx, bool state) {
    if (idx >= 2 || !relay_devices[idx]) return;
    esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(relay_devices[idx], ESP_RMAKER_DEF_POWER_NAME), esp_rmaker_bool(state));
}

// --- Initialization ---

esp_err_t app_rainmaker_init(void) {
    esp_rmaker_config_t rainmaker_cfg = { .enable_time_sync = true };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "NexusIR Hub", "Smart Gateway");
    if (!node) return ESP_FAIL;

    // 1. AC Device
#if !defined(CONFIG_APP_ESPNOW_AC_DISABLED)
    ac_device = esp_rmaker_device_create("Air Conditioner", ESP_RMAKER_DEVICE_AIR_CONDITIONER, NULL);
    esp_rmaker_device_add_param(ac_device, esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, false));
    esp_rmaker_device_add_param(ac_device, esp_rmaker_temperature_param_create(ESP_RMAKER_DEF_TEMPERATURE_NAME, 25.0));
    
    esp_rmaker_param_t *mode_p = esp_rmaker_param_create("Mode", ESP_RMAKER_PARAM_MODE, esp_rmaker_str("Auto"), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (mode_p) {
        static const char *mode_list[] = {"Auto", "Cool", "Heat", "Fan", "Dry"};
        esp_rmaker_param_add_valid_str_list(mode_p, mode_list, 5);
        esp_rmaker_param_add_ui_type(mode_p, ESP_RMAKER_UI_DROPDOWN);
        esp_rmaker_device_add_param(ac_device, mode_p);
    }
    
    esp_rmaker_param_t *ac_speed_p = esp_rmaker_param_create("Fan Speed", ESP_RMAKER_PARAM_SPEED, esp_rmaker_str("Auto"), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (ac_speed_p) {
        static const char *speed_list[] = {"Auto", "Low", "Medium", "High"};
        esp_rmaker_param_add_valid_str_list(ac_speed_p, speed_list, 4);
        esp_rmaker_param_add_ui_type(ac_speed_p, ESP_RMAKER_UI_DROPDOWN);
        esp_rmaker_device_add_param(ac_device, ac_speed_p);
    }
    
    esp_rmaker_param_t *brand_p = esp_rmaker_param_create("Brand", ESP_RMAKER_PARAM_NAME, esp_rmaker_str("Custom"), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_device_add_param(ac_device, brand_p);
    esp_rmaker_device_add_param(ac_device, esp_rmaker_param_create(PARAM_CUSTOM_BRAND_NAME, NULL, esp_rmaker_str("Custom"), PROP_FLAG_READ));

    esp_rmaker_device_add_cb(ac_device, ac_write_cb, NULL);
    esp_rmaker_node_add_device(node, ac_device);
#endif

    // 2. Fan Device
#if !defined(CONFIG_APP_ESPNOW_FAN_DISABLED)
    fan_device = esp_rmaker_device_create("Fan", ESP_RMAKER_DEVICE_FAN, NULL);
    esp_rmaker_device_add_param(fan_device, esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, false));
    
    esp_rmaker_param_t *fan_speed_p = esp_rmaker_param_create("Speed", ESP_RMAKER_PARAM_SPEED, esp_rmaker_str("Auto"), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (fan_speed_p) {
        static const char *speed_list[] = {"Auto", "Low", "Medium", "High"};
        esp_rmaker_param_add_valid_str_list(fan_speed_p, speed_list, 4);
        esp_rmaker_param_add_ui_type(fan_speed_p, ESP_RMAKER_UI_DROPDOWN);
        esp_rmaker_device_add_param(fan_device, fan_speed_p);
    }
    
    esp_rmaker_device_add_cb(fan_device, fan_write_cb, NULL);
    esp_rmaker_node_add_device(node, fan_device);
#endif

    // 3. LED Lamps (1-5)
    static const char *led_names[] = { 
        CONFIG_APP_LED1_NAME, 
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

    bool led_enabled[] = { 
#if !defined(CONFIG_APP_ESPNOW_LED1_DISABLED)
        true,
#else
        false,
#endif
#if !defined(CONFIG_APP_ESPNOW_LED2_DISABLED)
        true,
#else
        false,
#endif
#if !defined(CONFIG_APP_ESPNOW_LED3_DISABLED)
        true,
#else
        false,
#endif
#if !defined(CONFIG_APP_ESPNOW_LED4_DISABLED)
        true,
#else
        false,
#endif
#if !defined(CONFIG_APP_ESPNOW_LED5_DISABLED)
        true,
#else
        false,
#endif
    };

    for (int i = 0; i < 5; i++) {
        if (led_enabled[i]) {
            led_devices[i] = esp_rmaker_lightbulb_device_create(led_names[i], (void*)(uintptr_t)i, false);
            if (led_devices[i]) {
                esp_rmaker_device_add_cb(led_devices[i], led_write_cb, (void*)(uintptr_t)i);
                esp_rmaker_node_add_device(node, led_devices[i]);
            }
        }
    }

    // 4. Relays (1-2)
#if !defined(CONFIG_APP_ESPNOW_RELAY_DISABLED)
    static const char *relay_names[] = { CONFIG_APP_RELAY1_NAME, CONFIG_APP_RELAY2_NAME };
    for (int i = 0; i < 2; i++) {
        relay_devices[i] = esp_rmaker_device_create(relay_names[i], ESP_RMAKER_DEVICE_SWITCH, (void*)(uintptr_t)i);
        if (relay_devices[i]) {
            esp_rmaker_device_add_param(relay_devices[i], esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, false));
            esp_rmaker_device_add_cb(relay_devices[i], relay_write_cb, (void*)(uintptr_t)i);
            esp_rmaker_node_add_device(node, relay_devices[i]);
        }
    }
#endif

    // 5. Sensors & WebUI
    esp_rmaker_device_t *temp_sensor = esp_rmaker_device_create("Temperature", ESP_RMAKER_DEVICE_TEMP_SENSOR, NULL);
    if (temp_sensor) {
        esp_rmaker_device_add_param(temp_sensor, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "Temperature Sensor"));
        esp_rmaker_device_add_param(temp_sensor, esp_rmaker_param_create("Temperature", ESP_RMAKER_PARAM_TEMPERATURE, esp_rmaker_float(0.0), PROP_FLAG_READ));
        esp_rmaker_node_add_device(node, temp_sensor);
    }

    esp_rmaker_device_t *web_cfg = esp_rmaker_device_create("System Config", ESP_RMAKER_DEVICE_OTHER, NULL);
    if (web_cfg) {
        esp_rmaker_device_add_param(web_cfg, esp_rmaker_power_param_create(PARAM_WEBUI_MODE, false));
        esp_rmaker_device_add_cb(web_cfg, common_write_cb, NULL);
        esp_rmaker_node_add_device(node, web_cfg);
    }

    // 6. Dynamic IR Devices
    cJSON *brands = NULL;
    svc_nvs_load_custom_brands(&brands);
    if (brands && cJSON_IsArray(brands)) {
        cJSON *ir_keys = svc_nvs_get_ir_keys();
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, brands) {
            cJSON *name_item = cJSON_GetObjectItem(item, "name");
            cJSON *type_item = cJSON_GetObjectItem(item, "type");
            if (!cJSON_IsString(name_item) || !cJSON_IsString(type_item)) continue;
            
            const char *name = name_item->valuestring;
            const char *type = type_item->valuestring;

            char short_brand[16] = {0};
            int b_idx = 0;
            for (int i = 0; name[i] && b_idx < 15; i++) {
                char c = name[i];
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                    short_brand[b_idx++] = toupper((unsigned char)c);
            }

            if (strcmp(type, "TV") == 0) {
                esp_rmaker_device_t *tv_dev = esp_rmaker_device_create(name, ESP_RMAKER_DEVICE_SWITCH, strdup(short_brand));
                if (tv_dev) {
                    esp_rmaker_device_add_param(tv_dev, esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, false));
                    esp_rmaker_device_add_cb(tv_dev, tv_rmaker_write_cb, NULL);
                    esp_rmaker_node_add_device(node, tv_dev);
                }
            } else if (strcmp(type, "CUSTOM") == 0) {
                char prefix[32];
                snprintf(prefix, sizeof(prefix), "C_%s_", short_brand);
                int p_len = strlen(prefix);
                if (ir_keys && cJSON_IsArray(ir_keys)) {
                    cJSON *k_item = NULL;
                    cJSON_ArrayForEach(k_item, ir_keys) {
                        if (cJSON_IsString(k_item)) {
                            const char *key_str = k_item->valuestring;
                            if (strncmp(key_str, prefix, p_len) == 0) {
                                const char *suffix = key_str + p_len;
                                char dev_name[64];
                                snprintf(dev_name, sizeof(dev_name), "%s %s", name, suffix);
                                esp_rmaker_device_t *btn_dev = esp_rmaker_device_create(dev_name, ESP_RMAKER_DEVICE_SWITCH, strdup(key_str));
                                if (btn_dev) {
                                    esp_rmaker_device_add_param(btn_dev, esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, false));
                                    esp_rmaker_device_add_cb(btn_dev, custom_btn_rmaker_write_cb, NULL);
                                    esp_rmaker_node_add_device(node, btn_dev);
                                }
                            }
                        }
                    }
                }
            }
        }
        if (ir_keys) cJSON_Delete(ir_keys);
        cJSON_Delete(brands);
    }

    esp_rmaker_start();

    // 7. Initial State Sync
    ir_ac_state_t ac_s; mgr_ac_get_state(&ac_s); app_rainmaker_update_ac(&ac_s);
    ir_fan_state_t fan_s; mgr_fan_get_state(&fan_s); app_rainmaker_update_fan(&fan_s);
    for (int i = 0; i < 5; i++) {
        if (led_devices[i]) {
            uint8_t p, r, g, b, br, s; drv_led_effect_t e;
            if (drv_led_get_config(i, &p, &r, &g, &b, &e, &br, &s) == ESP_OK) {
                app_rainmaker_update_led(i, p, br, r, g, b);
            }
        }
    }
    for (int i = 0; i < 2; i++) {
        if (relay_devices[i]) {
            app_rainmaker_update_relay(i, mgr_relay_get_state(i));
        }
    }

    return ESP_OK;
}

void app_rainmaker_update_custom_brands(char **brands, size_t count) {
    if (!ac_device) return;
    esp_rmaker_param_t *brand_param = esp_rmaker_device_get_param_by_name(ac_device, "Brand");
    if (!brand_param) return;

    size_t total_count = count > 0 ? count : 1;
    char **new_list = malloc(total_count * sizeof(char *));
    if (!new_list) return;

    if (count == 0) new_list[0] = strdup("Custom");
    else {
        for (size_t i = 0; i < count; i++) new_list[i] = strdup(brands[i]);
    }

    char **old_list = g_brand_str_list;
    size_t old_count = g_brand_list_count;
    g_brand_str_list = new_list;
    g_brand_list_count = total_count;

    if (esp_rmaker_param_add_valid_str_list(brand_param, (const char **)new_list, total_count) == ESP_OK) {
        esp_rmaker_report_node_details();
    }

    if (old_list) {
        for (size_t i = 0; i < old_count; i++) if (old_list[i]) free(old_list[i]);
        free(old_list);
    }
}

#else

esp_err_t app_rainmaker_init(void) { return ESP_OK; }
void app_rainmaker_update_ac(const ir_ac_state_t *state) {}
void app_rainmaker_update_fan(const ir_fan_state_t *state) {}
void app_rainmaker_update_led(uint8_t lamp_id, uint8_t power, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b) {}
void app_rainmaker_update_relay(uint8_t idx, bool state) {}
void app_rainmaker_update_custom_brands(char **brands, size_t count) {}
void app_rainmaker_register_webui_toggle(webui_toggle_cb_t cb) {}

#endif
