#include "mgr_ac_logic.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mgr_ir_protocols.h" // Required for mgr_ir_send_key
#include "nvs.h"
#include "nvs_flash.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "drv_ir_rmt.h"
#include "esp_err.h"

static const char *TAG = "mgr_ac_logic";

#define AC_NVS_NAMESPACE "ac_config"
#define AC_KEY_BRAND "brand"
#define AC_KEY_IS_CUSTOM "is_custom"
#define AC_KEY_CUSTOM_NAME "custom_name"
#define AC_KEY_STATE "state"

static ir_ac_state_t g_ac_state = {.power = false,
                                   .mode = 1, // Cool
                                   .temp = 24,
                                   .fan = 0,     // Auto
                                   .swing_v = 0, // Off
                                   .swing_h = 0};

static ac_brand_t g_ac_brand = AC_BRAND_CUSTOM;

// Custom brand support
static char g_custom_brand_name[32] = {0};
static bool g_is_custom_brand = false;

// State tracking for custom logical sending
static ir_ac_state_t g_last_sent_state = {
    .power = false, .mode = 1, .temp = 24, .fan = 0};

/**
 * @brief Load AC state from NVS
 */
static esp_err_t load_state_from_nvs(void) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(AC_NVS_NAMESPACE, NVS_READONLY, &nvs);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "NVS namespace not found, using defaults");
    return err;
  }

  // Load brand
  uint8_t brand = 0;
  err = nvs_get_u8(nvs, AC_KEY_BRAND, &brand);
  if (err == ESP_OK && brand < AC_BRAND_MAX) {
    g_ac_brand = (ac_brand_t)brand;
    ESP_LOGI(TAG, "Loaded brand: %d", brand);
  }

  // Load custom brand flag
  uint8_t is_custom = 0;
  if (nvs_get_u8(nvs, AC_KEY_IS_CUSTOM, &is_custom) == ESP_OK) {
    g_is_custom_brand = (is_custom != 0);
  }

  // Load custom brand name
  if (g_is_custom_brand) {
    size_t len = sizeof(g_custom_brand_name);
    nvs_get_str(nvs, AC_KEY_CUSTOM_NAME, g_custom_brand_name, &len);
    ESP_LOGI(TAG, "Loaded custom brand: %s", g_custom_brand_name);
  }

  // Load AC state
  size_t state_len = sizeof(ir_ac_state_t);
  err = nvs_get_blob(nvs, AC_KEY_STATE, &g_ac_state, &state_len);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Loaded state: P=%d, M=%d, T=%d", g_ac_state.power,
             g_ac_state.mode, g_ac_state.temp);
    // Sync last sent state to loaded state to avoid initial burst of commands
    memcpy(&g_last_sent_state, &g_ac_state, sizeof(ir_ac_state_t));
  }

  nvs_close(nvs);
  return ESP_OK;
}

/**
 * @brief Save AC state to NVS
 */
static esp_err_t save_state_to_nvs(void) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(AC_NVS_NAMESPACE, NVS_READWRITE, &nvs);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    return err;
  }

  // Save brand
  nvs_set_u8(nvs, AC_KEY_BRAND, (uint8_t)g_ac_brand);

  // Save custom brand info
  nvs_set_u8(nvs, AC_KEY_IS_CUSTOM, g_is_custom_brand ? 1 : 0);
  if (g_is_custom_brand) {
    nvs_set_str(nvs, AC_KEY_CUSTOM_NAME, g_custom_brand_name);
  }

  // Save state
  nvs_set_blob(nvs, AC_KEY_STATE, &g_ac_state, sizeof(ir_ac_state_t));

  err = nvs_commit(nvs);
  nvs_close(nvs);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "State saved to NVS");
  }
  return err;
}

static void ac_espnow_handler(const ir_ac_state_t *state, ac_brand_t brand,
                              const char *custom_name) {
  ESP_LOGI(TAG, "Syncing AC state from ESP-NOW...");
  mgr_ac_set_state(state);
  mgr_ac_set_brand(brand);
  if (custom_name) {
    mgr_ac_set_custom_brand(custom_name);
  }
  // Trigger local hardware IR
  mgr_ac_send();
}

void mgr_ac_init(void) {
  ESP_LOGI(TAG, "AC Logic Initializing...");

  // Load state from NVS
  esp_err_t err = load_state_from_nvs();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Using default AC state");
  }

  ESP_LOGI(TAG, "AC initialized: Brand=%d, Custom=%d, Power=%d", g_ac_brand,
           g_is_custom_brand, g_ac_state.power);
}

void mgr_ac_set_state(const ir_ac_state_t *state) {
  if (state) {
    memcpy(&g_ac_state, state, sizeof(ir_ac_state_t));
    save_state_to_nvs(); // Auto-save on change
  }
}

void mgr_ac_get_state(ir_ac_state_t *state) {
  if (state) {
    memcpy(state, &g_ac_state, sizeof(ir_ac_state_t));
  }
}

void mgr_ac_set_brand(ac_brand_t brand) {
  if (brand < AC_BRAND_MAX) {
    g_ac_brand = brand;
    g_is_custom_brand = false; // Clear custom brand flag
    save_state_to_nvs();       // Auto-save on change
  }
}

ac_brand_t mgr_ac_get_brand(void) { return g_ac_brand; }

void mgr_ac_set_custom_brand(const char *name) {
  if (name && strlen(name) > 0) {
    strncpy(g_custom_brand_name, name, sizeof(g_custom_brand_name) - 1);
    g_custom_brand_name[sizeof(g_custom_brand_name) - 1] = '\0';
    g_is_custom_brand = true;
    ESP_LOGI(TAG, "Set custom AC brand: %s", g_custom_brand_name);
    save_state_to_nvs(); // Auto-save on change
  }
}

const char *mgr_ac_get_custom_brand(void) {
  return g_is_custom_brand ? g_custom_brand_name : NULL;
}

bool mgr_ac_is_custom_brand(void) { return g_is_custom_brand; }

static void send_custom_key_suffix(const char *brand, const char *suffix) {
  char short_brand[16] = {0};
  int idx = 0;
  for (int i = 0; brand[i] && idx < 15; i++) {
    char c = brand[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
      short_brand[idx++] = toupper((int)c);
    }
  }

  char key[32];
  snprintf(key, sizeof(key), "A_%s_%s", short_brand, suffix);

  // Try sending
  if (mgr_ir_send_key(key) != ESP_OK) {
    ESP_LOGW(TAG, "Custom key not found: %s", key);
  } else {
    ESP_LOGI(TAG, "Sent custom key: %s", key);
  }
}

static mgr_ac_bridge_cb_t s_bridge_cb = NULL;

void mgr_ac_set_bridge_cb(mgr_ac_bridge_cb_t cb) { s_bridge_cb = cb; }

esp_err_t mgr_ac_send(void) {
  ESP_LOGI(TAG, "Syncing state via callback if set");
  if (s_bridge_cb) {
    s_bridge_cb(&g_ac_state, 0,
                g_is_custom_brand ? g_custom_brand_name : "AC");
  }

  const char *dev_name = g_is_custom_brand ? g_custom_brand_name : "AC";

  ESP_LOGI(TAG, "Sending AC Command for %s: P=%d, M=%d, T=%d", dev_name,
           g_ac_state.power, g_ac_state.mode, g_ac_state.temp);

  // Matrix Capture Support (Primary Strategy)
  if (mgr_ir_matrix_exists(dev_name)) {
    int index = 0;
    if (!g_ac_state.power) {
      index = 0; // Off
    } else {
      // Map 16-30C to index 1-15
      index = g_ac_state.temp - 16 + 1;
      if (index < 1)
        index = 1;
      if (index > 15)
        index = 15;
    }
    ESP_LOGI(TAG, "Matrix Match! Sending entry %d for %s", index, dev_name);
    return mgr_ir_send_from_matrix(dev_name, index);
  }

  // Legacy Key Suffix Fallback (for single keys like A_LivingRoom_ON)
  if (g_ac_state.power) {
    send_custom_key_suffix(
        dev_name, mgr_ir_send_key_exists("A_", dev_name, "ON") ? "ON" : "PWR");
  } else {
    send_custom_key_suffix(
        dev_name,
        mgr_ir_send_key_exists("A_", dev_name, "OFF") ? "OFF" : "PWR");
  }
  return ESP_OK;

  if (g_ac_state.power) {
    // Mode Logic
    if (g_ac_state.mode != g_last_sent_state.mode) {
      send_custom_key_suffix(dev_name, "MOD");
    }

    // Temp Logic
    if (g_ac_state.temp != g_last_sent_state.temp) {
      char temp_suffix[8];
      snprintf(temp_suffix, sizeof(temp_suffix), "T%d", g_ac_state.temp);
      send_custom_key_suffix(dev_name, temp_suffix);
    }

    // Fan Logic
    if (g_ac_state.fan != g_last_sent_state.fan) {
      send_custom_key_suffix(dev_name, "FAN");
    }
  }

  // Update last state
  memcpy(&g_last_sent_state, &g_ac_state, sizeof(ir_ac_state_t));
  return ESP_OK;
}
