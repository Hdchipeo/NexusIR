#include "mgr_fan_logic.h"
#include "esp_log.h"
#include "mgr_ir_protocols.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "sdkconfig.h"

static const char *TAG = "mgr_fan_logic";

#define FAN_NVS_NAMESPACE "fan_config"
#define FAN_KEY_BRAND "brand"
#define FAN_KEY_IS_CUSTOM "is_custom"
#define FAN_KEY_CUSTOM_NAME "custom_name"
#define FAN_KEY_STATE "state"

static ir_fan_state_t g_fan_state = {.power = false, .speed = 1, .swing = false};
static fan_brand_t g_fan_brand = FAN_BRAND_GENERIC;
static char g_custom_brand_name[32] = "";
static bool g_is_custom_brand = false;

static ir_fan_state_t g_last_sent_state = {.power = false, .speed = 1, .swing = false};
static mgr_fan_bridge_cb_t s_bridge_cb = NULL;

static esp_err_t save_state_to_nvs(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(FAN_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for saving: %s", esp_err_to_name(err));
        return err;
    }
    nvs_set_u8(nvs, FAN_KEY_BRAND, (uint8_t)g_fan_brand);
    nvs_set_u8(nvs, FAN_KEY_IS_CUSTOM, g_is_custom_brand ? 1 : 0);
    if (g_is_custom_brand && strlen(g_custom_brand_name) > 0) {
        nvs_set_str(nvs, FAN_KEY_CUSTOM_NAME, g_custom_brand_name);
    }
    nvs_set_blob(nvs, FAN_KEY_STATE, &g_fan_state, sizeof(ir_fan_state_t));
    err = nvs_commit(nvs);
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "Fan state saved: Brand=%d, Custom=%d, Name='%s'", 
             (int)g_fan_brand, g_is_custom_brand, g_custom_brand_name);
    return err;
}

static esp_err_t load_state_from_nvs(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(FAN_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace %s not found", FAN_NVS_NAMESPACE);
        return err;
    }
    
    uint8_t brand = 0;
    if (nvs_get_u8(nvs, FAN_KEY_BRAND, &brand) == ESP_OK) g_fan_brand = (fan_brand_t)brand;
    
    uint8_t is_custom = 0;
    if (nvs_get_u8(nvs, FAN_KEY_IS_CUSTOM, &is_custom) == ESP_OK) g_is_custom_brand = (is_custom != 0);
    
    if (g_is_custom_brand) {
        size_t len = sizeof(g_custom_brand_name);
        memset(g_custom_brand_name, 0, len);
        if (nvs_get_str(nvs, FAN_KEY_CUSTOM_NAME, g_custom_brand_name, &len) == ESP_OK) {
            ESP_LOGI(TAG, "Loaded custom fan brand: %s", g_custom_brand_name);
        } else {
            g_is_custom_brand = false; // Fallback if name missing
        }
    }
    
    size_t state_len = sizeof(ir_fan_state_t);
    nvs_get_blob(nvs, FAN_KEY_STATE, &g_fan_state, &state_len);
    nvs_close(nvs);
    
    memcpy(&g_last_sent_state, &g_fan_state, sizeof(ir_fan_state_t));
    return ESP_OK;
}

void mgr_fan_init(void) {
    ESP_LOGI(TAG, "Initializing Fan Logic...");
    load_state_from_nvs();

    // Default to custom brand "Fan" if no brand was saved
    if (!g_is_custom_brand || g_custom_brand_name[0] == '\0') {
        strncpy(g_custom_brand_name, "Fan", sizeof(g_custom_brand_name) - 1);
        g_custom_brand_name[sizeof(g_custom_brand_name) - 1] = '\0';
        g_is_custom_brand = true;
        save_state_to_nvs();
        ESP_LOGI(TAG, "No brand saved, defaulting to custom brand: 'Fan'");
    }

    ESP_LOGI(TAG, "Fan Logic Initialized: P=%d, S=%d, Brand=%d, Custom=%d (%s)", 
             g_fan_state.power, g_fan_state.speed, (int)g_fan_brand, g_is_custom_brand, g_custom_brand_name);
}

void mgr_fan_set_state(const ir_fan_state_t *state) {
    if (state) {
        memcpy(&g_fan_state, state, sizeof(ir_fan_state_t));
        save_state_to_nvs();
    }
}

void mgr_fan_get_state(ir_fan_state_t *state) {
    if (state) memcpy(state, &g_fan_state, sizeof(ir_fan_state_t));
}

void mgr_fan_set_brand(fan_brand_t brand) {
    ESP_LOGI(TAG, "Setting fan brand: %d", (int)brand);
    g_fan_brand = brand;
    g_is_custom_brand = false;
    save_state_to_nvs();
}

fan_brand_t mgr_fan_get_brand(void) { return g_fan_brand; }

void mgr_fan_set_custom_brand(const char *name) {
    if (name) {
        ESP_LOGI(TAG, "Setting custom fan brand: %s", name);
        strncpy(g_custom_brand_name, name, sizeof(g_custom_brand_name) - 1);
        g_custom_brand_name[sizeof(g_custom_brand_name) - 1] = '\0';
        g_is_custom_brand = true;
        save_state_to_nvs();
    }
}

const char *mgr_fan_get_custom_brand(void) { return g_is_custom_brand ? g_custom_brand_name : NULL; }
bool mgr_fan_is_custom_brand(void) { return g_is_custom_brand; }
void mgr_fan_set_bridge_cb(mgr_fan_bridge_cb_t cb) { s_bridge_cb = cb; }

static void send_custom_key(const char *brand, const char *suffix) {
    char short_brand[16] = {0};
    int idx = 0;
    for (int i = 0; brand[i] && idx < 15; i++) {
        char c = brand[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            short_brand[idx++] = toupper((int)c);
        }
    }
    char key[32];
    snprintf(key, sizeof(key), "F_%s_%s", short_brand, suffix);
    if (mgr_ir_send_key(key) != ESP_OK) {
        ESP_LOGW(TAG, "Fan key not found: %s", key);
    } else {
        ESP_LOGI(TAG, "Sent Fan key: %s", key);
    }
}

esp_err_t mgr_fan_send(void) {
    if (s_bridge_cb) {
        s_bridge_cb(&g_fan_state, g_fan_brand, g_is_custom_brand ? g_custom_brand_name : NULL);
    }

    ESP_LOGI(TAG, "mgr_fan_send: Power=%d, Speed=%d, Brand=%d, Custom=%d (%s)", 
             g_fan_state.power, g_fan_state.speed, (int)g_fan_brand, g_is_custom_brand, g_custom_brand_name);

    if (g_is_custom_brand || g_fan_brand == FAN_BRAND_GENERIC) {
        const char *brand = g_is_custom_brand ? g_custom_brand_name : "Fan";
        bool power_changed = (g_fan_state.power != g_last_sent_state.power);
        bool speed_changed = (g_fan_state.speed != g_last_sent_state.speed);
        bool swing_changed = (g_fan_state.swing != g_last_sent_state.swing);

        ESP_LOGI(TAG, "Delta: pwr=%d spd=%d swg=%d", power_changed, speed_changed, swing_changed);

        // Power: Only send when power state actually changed
        if (power_changed) {
            if (g_fan_state.power) {
                // Turning ON
                if (mgr_ir_send_key_exists("F_", brand, "ON")) {
                    send_custom_key(brand, "ON");
                } else if (mgr_ir_send_key_exists("F_", brand, "BAT")) {
                    send_custom_key(brand, "BAT");
                } else if (mgr_ir_send_key_exists("F_", brand, "NGUON")) {
                    send_custom_key(brand, "NGUON");
                } else {
                    send_custom_key(brand, "PWR");
                }
            } else {
                // Turning OFF
                if (mgr_ir_send_key_exists("F_", brand, "OFF")) {
                    send_custom_key(brand, "OFF");
                } else if (mgr_ir_send_key_exists("F_", brand, "TAT")) {
                    send_custom_key(brand, "TAT");
                } else if (mgr_ir_send_key_exists("F_", brand, "NGUON")) {
                    send_custom_key(brand, "NGUON");
                } else {
                    send_custom_key(brand, "PWR");
                }
            }
        }

        // Speed/Swing: Only send if power is ON and value changed
        if (g_fan_state.power && speed_changed) {
            char spd_suffix[8];
            snprintf(spd_suffix, sizeof(spd_suffix), "S%d", g_fan_state.speed);
            if (mgr_ir_send_key_exists("F_", brand, spd_suffix)) {
                send_custom_key(brand, spd_suffix);
            } else {
                send_custom_key(brand, "SUP");
            }
        }

        if (g_fan_state.power && swing_changed) {
            send_custom_key(brand, "SWG");
        }

        memcpy(&g_last_sent_state, &g_fan_state, sizeof(ir_fan_state_t));
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Preset Brand %d selected, but no internal IR protocol implemented yet. Please use 'Custom' brand and learn commands.", (int)g_fan_brand);
    return ESP_ERR_NOT_SUPPORTED;
}

bool mgr_fan_is_configured(void) {
#ifdef CONFIG_APP_ESPNOW_FAN_DISABLED
    return false;
#else
    const char *brand = g_is_custom_brand ? g_custom_brand_name : "Fan";
    if (mgr_ir_send_key_exists("F_", brand, "ON") ||
        mgr_ir_send_key_exists("F_", brand, "OFF") ||
        mgr_ir_send_key_exists("F_", brand, "BAT") ||
        mgr_ir_send_key_exists("F_", brand, "NGUON") ||
        mgr_ir_send_key_exists("F_", brand, "PWR")) {
        return true;
    }
    return false;
#endif
}
