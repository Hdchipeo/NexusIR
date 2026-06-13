#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "svc_espnow.h"
#include "drv_led.h"
#include "mgr_display.h"

#if CONFIG_APP_ESPNOW_BRIDGE_ENABLE || CONFIG_APP_ESPNOW_SLAVE_ENABLE

static const char *TAG = "svc_espnow";

#define PEER_NVS_NAMESPACE "espnow_ctrl"
#define PEER_NVS_KEY "peers"
#define MAX_PEERS 20

typedef struct {
  uint8_t type; // 0: AC, 1: LED, 2: FAN, 3: TEMP
  union {
    struct {
      uint8_t power;
      uint8_t mode;
      uint8_t temp;
      uint8_t fan;
      uint8_t brand;
      char custom_name[32];
    } ac;
    struct {
      uint8_t lamp_id;
      uint8_t power;
      uint8_t effect;
      uint8_t brightness;
      uint8_t r;
      uint8_t g;
      uint8_t b;
      uint8_t speed;
    } led;
    struct {
      uint8_t power;
      uint8_t speed;
      uint8_t swing;
      uint8_t brand;
      char custom_name[32];
    } fan;
    struct {
      int16_t temp_x10;   // °C × 10 (e.g., 25.3 → 253)
      int16_t humid_x10;  // % × 10
    } temp;
    struct {
      uint8_t idx;
      uint8_t state;
    } relay;
    struct {
      char device_name[32];
    } discovery;
  };
} espnow_packet_t;

static uint8_t s_target_mac[6];

#ifdef CONFIG_APP_ESPNOW_AC_MAC
static uint8_t s_ac_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#endif
#ifdef CONFIG_APP_ESPNOW_FAN_MAC
static uint8_t s_fan_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#endif
static uint8_t s_led_mac[5][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};
static uint8_t s_relay_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool s_espnow_initialized = false;

static espnow_ac_handler_t s_ac_handler = NULL;
static espnow_led_handler_t s_led_handler = NULL;
static espnow_fan_handler_t s_fan_handler = NULL;
static espnow_temp_handler_t s_temp_handler = NULL;
static espnow_relay_handler_t s_relay_handler = NULL;

static uint8_t s_peer_list[MAX_PEERS][6];
static int s_peer_count = 0;

static bool s_is_scanning = false;
#define MAX_DISCOVERED_DEVICES 15
static espnow_discovered_device_t s_discovered_devices[MAX_DISCOVERED_DEVICES];
static int s_discovered_count = 0;

static void add_to_discovered_list(const uint8_t *mac, const char *name) {
  for (int i = 0; i < s_discovered_count; i++) {
    if (memcmp(s_discovered_devices[i].mac, mac, 6) == 0) {
      return;
    }
  }
  if (s_discovered_count < MAX_DISCOVERED_DEVICES) {
    memcpy(s_discovered_devices[s_discovered_count].mac, mac, 6);
    strncpy(s_discovered_devices[s_discovered_count].name, name, sizeof(s_discovered_devices[s_discovered_count].name) - 1);
    s_discovered_devices[s_discovered_count].name[sizeof(s_discovered_devices[s_discovered_count].name) - 1] = '\0';
    s_discovered_count++;
  }
}

void svc_espnow_register_ac_handler(espnow_ac_handler_t handler) {
  s_ac_handler = handler;
}

void svc_espnow_register_led_handler(espnow_led_handler_t handler) {
  s_led_handler = handler;
}

void svc_espnow_register_fan_handler(espnow_fan_handler_t handler) {
  s_fan_handler = handler;
}

void svc_espnow_register_temp_handler(espnow_temp_handler_t handler) {
  s_temp_handler = handler;
}

void svc_espnow_register_relay_handler(espnow_relay_handler_t handler) {
  s_relay_handler = handler;
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,
                           const uint8_t *data, int len) {
  mgr_display_wake();
  if (len != sizeof(espnow_packet_t)) {
    ESP_LOGW(TAG, "Received invalid ESP-NOW packet size: %d", len);
    return;
  }

  espnow_packet_t *pkt = (espnow_packet_t *)data;

  if (pkt->type == ESPNOW_TYPE_AC) {
    ESP_LOGI(TAG, "Received AC state via ESP-NOW from " MACSTR, MAC2STR(recv_info->src_addr));
    if (s_ac_handler) {
      ir_ac_state_t state;
      memset(&state, 0, sizeof(state));
      state.power = pkt->ac.power;
      state.mode = pkt->ac.mode;
      state.temp = pkt->ac.temp;
      state.fan = pkt->ac.fan;
      state.swing_v = 0;
      state.swing_h = 0;
      s_ac_handler(&state, (ac_brand_t)pkt->ac.brand,
                   strlen(pkt->ac.custom_name) > 0 ? pkt->ac.custom_name : NULL);
    }
  } else if (pkt->type == ESPNOW_TYPE_LED) {
    ESP_LOGI(TAG, "Received LED state via ESP-NOW from " MACSTR, MAC2STR(recv_info->src_addr));
    if (s_led_handler) {
      s_led_handler(pkt->led.lamp_id, pkt->led.power, pkt->led.effect, pkt->led.brightness,
                    pkt->led.r, pkt->led.g, pkt->led.b, pkt->led.speed);
    }
  } else if (pkt->type == ESPNOW_TYPE_FAN) {
    ESP_LOGI(TAG, "Received FAN state via ESP-NOW from " MACSTR, MAC2STR(recv_info->src_addr));
    if (s_fan_handler) {
      ir_fan_state_t state;
      state.power = pkt->fan.power;
      state.speed = pkt->fan.speed;
      state.swing = pkt->fan.swing;
      s_fan_handler(&state, (fan_brand_t)pkt->fan.brand,
                    strlen(pkt->fan.custom_name) > 0 ? pkt->fan.custom_name : NULL);
    }
  } else if (pkt->type == ESPNOW_TYPE_TEMP) {
    float temp = pkt->temp.temp_x10 / 10.0f;
    float humid = pkt->temp.humid_x10 / 10.0f;
    ESP_LOGI(TAG, "Received TEMP via ESP-NOW: %.1f°C, %.1f%%", temp, humid);
    if (s_temp_handler) {
      s_temp_handler(temp, humid);
    }
  } else if (pkt->type == ESPNOW_TYPE_RELAY) {
    ESP_LOGI(TAG, "Received RELAY state via ESP-NOW from " MACSTR, MAC2STR(recv_info->src_addr));
    if (s_relay_handler) {
      s_relay_handler(pkt->relay.idx, pkt->relay.state);
    }
  } else if (pkt->type == ESPNOW_TYPE_DISCOVERY_REQ) {
    ESP_LOGI(TAG, "Received ESP-NOW Discovery Request from " MACSTR, MAC2STR(recv_info->src_addr));
    espnow_packet_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.type = ESPNOW_TYPE_DISCOVERY_RESP;
    strncpy(resp.discovery.device_name, CONFIG_APP_DEVICE_NAME, sizeof(resp.discovery.device_name) - 1);

    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, recv_info->src_addr, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    peer_info.ifidx = WIFI_IF_STA;
    esp_now_add_peer(&peer_info);

    peer_info.ifidx = WIFI_IF_AP;
    esp_now_add_peer(&peer_info);

    esp_now_send(recv_info->src_addr, (uint8_t *)&resp, sizeof(resp));
  } else if (pkt->type == ESPNOW_TYPE_DISCOVERY_RESP) {
    ESP_LOGI(TAG, "Received ESP-NOW Discovery Response from " MACSTR ": %s", MAC2STR(recv_info->src_addr), pkt->discovery.device_name);
    if (s_is_scanning) {
      add_to_discovered_list(recv_info->src_addr, pkt->discovery.device_name);
    }
  }
}

static bool parse_mac_address(const char *str, uint8_t *mac) {
  if (!str || strlen(str) != 17)
    return false;
  int values[6];
  if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x", &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) == 6) {
    for (int i = 0; i < 6; i++)
      mac[i] = (uint8_t)values[i];
    return true;
  }
  return false;
}

static void load_peers_from_nvs(void) {
  nvs_handle_t nvs;
  if (nvs_open(PEER_NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
    size_t size = sizeof(s_peer_list);
    if (nvs_get_blob(nvs, PEER_NVS_KEY, s_peer_list, &size) == ESP_OK) {
      s_peer_count = size / 6;
      ESP_LOGI(TAG, "Loaded %d peers from NVS", s_peer_count);
      for (int i = 0; i < s_peer_count; i++) {
        esp_now_peer_info_t peer_info = {0};
        memcpy(peer_info.peer_addr, s_peer_list[i], 6);
        peer_info.channel = 0;
        peer_info.encrypt = false;
        esp_now_add_peer(&peer_info);
      }
    }
    nvs_close(nvs);
  }
}

static void save_peers_to_nvs(void) {
  nvs_handle_t nvs;
  if (nvs_open(PEER_NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
    nvs_set_blob(nvs, PEER_NVS_KEY, s_peer_list, s_peer_count * 6);
    nvs_commit(nvs);
    nvs_close(nvs);
  }
}

esp_err_t svc_espnow_add_peer(const uint8_t *mac) {
  if (s_peer_count >= MAX_PEERS) return ESP_ERR_NO_MEM;
  
  // Check if already exists
  for (int i = 0; i < s_peer_count; i++) {
    if (memcmp(s_peer_list[i], mac, 6) == 0) return ESP_OK;
  }

  esp_now_peer_info_t peer_info = {0};
  memcpy(peer_info.peer_addr, mac, 6);
  peer_info.channel = 0;
  peer_info.encrypt = false;
  
  peer_info.ifidx = WIFI_IF_STA;
  esp_err_t err_sta = esp_now_add_peer(&peer_info);

  peer_info.ifidx = WIFI_IF_AP;
  esp_err_t err_ap = esp_now_add_peer(&peer_info);

  bool success = (err_sta == ESP_OK || err_sta == ESP_ERR_ESPNOW_EXIST ||
                  err_ap == ESP_OK || err_ap == ESP_ERR_ESPNOW_EXIST);

  if (success) {
    memcpy(s_peer_list[s_peer_count++], mac, 6);
    save_peers_to_nvs();
    return ESP_OK;
  }
  return err_sta;
}

esp_err_t svc_espnow_remove_peer(const uint8_t *mac) {
  esp_err_t err = esp_now_del_peer(mac);
  if (err == ESP_OK) {
    for (int i = 0; i < s_peer_count; i++) {
      if (memcmp(s_peer_list[i], mac, 6) == 0) {
        if (i < s_peer_count - 1) {
          memmove(s_peer_list[i], s_peer_list[i+1], (s_peer_count - i - 1) * 6);
        }
        s_peer_count--;
        save_peers_to_nvs();
        break;
      }
    }
  }
  return err;
}

esp_err_t svc_espnow_get_peers(uint8_t *mac_list, int *count) {
  if (!mac_list || !count) return ESP_ERR_INVALID_ARG;
  *count = s_peer_count;
  memcpy(mac_list, s_peer_list, s_peer_count * 6);
  return ESP_OK;
}

esp_err_t svc_espnow_init(void) {
  ESP_LOGI(TAG, "Initializing ESP-NOW Service...");

  ESP_ERROR_CHECK(esp_now_init());

  ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
  ESP_LOGI(TAG, "ESP-NOW listener active.");

  // Default peer from config
  if (!parse_mac_address(CONFIG_APP_ESPNOW_PEER_MAC, s_target_mac)) {
    ESP_LOGW(TAG, "Invalid Target MAC: %s. Using Broadcast.", CONFIG_APP_ESPNOW_PEER_MAC);
    memset(s_target_mac, 0xFF, 6);
  }

  // Helper lambda for adding peers
  void add_device_peer(const char* mac_str, uint8_t *mac_out) {
    uint8_t mac[6];
    if (parse_mac_address(mac_str, mac)) {
      // Ignore broadcast MAC
      uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      if (memcmp(mac, broadcast, 6) == 0) return;

      svc_espnow_add_peer(mac);
      if (mac_out) memcpy(mac_out, mac, 6);
    } else {
      if (mac_out) memcpy(mac_out, s_target_mac, 6);
    }
  }

#ifdef CONFIG_APP_ESPNOW_AC_MAC
  add_device_peer(CONFIG_APP_ESPNOW_AC_MAC, s_ac_mac);
#endif
#ifdef CONFIG_APP_ESPNOW_FAN_MAC
  add_device_peer(CONFIG_APP_ESPNOW_FAN_MAC, s_fan_mac);
#endif
#ifdef CONFIG_APP_ESPNOW_LED1_MAC
  add_device_peer(CONFIG_APP_ESPNOW_LED1_MAC, s_led_mac[0]);
#endif
#ifdef CONFIG_APP_ESPNOW_LED2_MAC
  add_device_peer(CONFIG_APP_ESPNOW_LED2_MAC, s_led_mac[1]);
#endif
#ifdef CONFIG_APP_ESPNOW_LED3_MAC
  add_device_peer(CONFIG_APP_ESPNOW_LED3_MAC, s_led_mac[2]);
#endif
#ifdef CONFIG_APP_ESPNOW_LED4_MAC
  add_device_peer(CONFIG_APP_ESPNOW_LED4_MAC, s_led_mac[3]);
#endif
#ifdef CONFIG_APP_ESPNOW_LED5_MAC
  add_device_peer(CONFIG_APP_ESPNOW_LED5_MAC, s_led_mac[4]);
#endif
#ifdef CONFIG_APP_ESPNOW_RELAY_MAC
  add_device_peer(CONFIG_APP_ESPNOW_RELAY_MAC, s_relay_mac);
#endif

  // --- Sync Peers from Kconfig ---
  uint8_t dummy_mac[6];
  add_device_peer(CONFIG_APP_ESPNOW_SYNC_PEER_1, dummy_mac);
  add_device_peer(CONFIG_APP_ESPNOW_SYNC_PEER_2, dummy_mac);
  add_device_peer(CONFIG_APP_ESPNOW_SYNC_PEER_3, dummy_mac);
  add_device_peer(CONFIG_APP_ESPNOW_SYNC_PEER_4, dummy_mac);
  add_device_peer(CONFIG_APP_ESPNOW_SYNC_PEER_5, dummy_mac);

  load_peers_from_nvs();

  s_espnow_initialized = true;
  return ESP_OK;
}

esp_err_t svc_espnow_bridge_ac_send(const ir_ac_state_t *state, ac_brand_t brand, 
                                  const char *custom_name) {
  if (!s_espnow_initialized) return ESP_ERR_INVALID_STATE;
  
  espnow_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = ESPNOW_TYPE_AC;
  pkt.ac.power = state->power;
  pkt.ac.mode = state->mode;
  pkt.ac.temp = state->temp;
  pkt.ac.fan = state->fan;
  pkt.ac.brand = (uint8_t)brand;
  if (custom_name) {
    strncpy(pkt.ac.custom_name, custom_name, sizeof(pkt.ac.custom_name) - 1);
  }

#ifdef CONFIG_APP_ESPNOW_AC_MAC
  uint8_t *mac = s_ac_mac;
#else
  uint8_t *mac = s_target_mac;
#endif
  ESP_LOGI(TAG, "Bridging AC update over ESP-NOW to default " MACSTR, MAC2STR(mac));
  esp_now_send(mac, (uint8_t *)&pkt, sizeof(pkt));

  // Also send to all dynamic peers
  for (int i = 0; i < s_peer_count; i++) {
    if (memcmp(s_peer_list[i], s_target_mac, 6) == 0) continue; // Skip if same
    ESP_LOGI(TAG, "Bridging AC update over ESP-NOW to dynamic node " MACSTR, MAC2STR(s_peer_list[i]));
    esp_now_send(s_peer_list[i], (uint8_t *)&pkt, sizeof(pkt));
  }
  return ESP_OK;
}

esp_err_t svc_espnow_bridge_led_send(uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness,
                                    uint8_t r, uint8_t g, uint8_t b, uint8_t speed) {
  uint8_t *mac = s_target_mac;
  if (lamp_id < 5) mac = s_led_mac[lamp_id];
  esp_err_t err = svc_espnow_bridge_led_send_to(mac, lamp_id, power, effect, brightness, r, g, b, speed);
  
  // Also send to all dynamic peers
  for (int i = 0; i < s_peer_count; i++) {
    if (memcmp(s_peer_list[i], s_target_mac, 6) == 0) continue; // Skip if same
    svc_espnow_bridge_led_send_to(s_peer_list[i], lamp_id, power, effect, brightness, r, g, b, speed);
  }
  return err;
}

esp_err_t svc_espnow_bridge_led_send_to(const uint8_t *mac, uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t speed) {
  if (!s_espnow_initialized) return ESP_ERR_INVALID_STATE;
  
  espnow_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = ESPNOW_TYPE_LED;
  pkt.led.lamp_id = lamp_id;
  pkt.led.power = power;
  pkt.led.effect = effect;
  pkt.led.brightness = brightness;
  pkt.led.r = r;
  pkt.led.g = g;
  pkt.led.b = b;
  pkt.led.speed = speed;

  ESP_LOGI(TAG, "Bridging LED update over ESP-NOW to " MACSTR, MAC2STR(mac));
  return esp_now_send(mac, (uint8_t *)&pkt, sizeof(pkt));
}

esp_err_t svc_espnow_bridge_fan_send(const ir_fan_state_t *state, fan_brand_t brand, 
                                   const char *custom_name) {
  if (!s_espnow_initialized) return ESP_ERR_INVALID_STATE;
  
  espnow_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = ESPNOW_TYPE_FAN;
  pkt.fan.power = state->power;
  pkt.fan.speed = state->speed;
  pkt.fan.swing = state->swing;
  pkt.fan.brand = (uint8_t)brand;
  if (custom_name) {
    strncpy(pkt.fan.custom_name, custom_name, sizeof(pkt.fan.custom_name) - 1);
  }

#ifdef CONFIG_APP_ESPNOW_FAN_MAC
  uint8_t *mac = s_fan_mac;
#else
  uint8_t *mac = s_target_mac;
#endif
  ESP_LOGI(TAG, "Bridging FAN update over ESP-NOW to default " MACSTR, MAC2STR(mac));
  esp_now_send(mac, (uint8_t *)&pkt, sizeof(pkt));

  // Also send to all dynamic peers
  for (int i = 0; i < s_peer_count; i++) {
    if (memcmp(s_peer_list[i], s_target_mac, 6) == 0) continue; // Skip if same
    ESP_LOGI(TAG, "Bridging FAN update over ESP-NOW to dynamic node " MACSTR, MAC2STR(s_peer_list[i]));
    esp_now_send(s_peer_list[i], (uint8_t *)&pkt, sizeof(pkt));
  }
  return ESP_OK;
}

esp_err_t svc_espnow_bridge_temp_send(float temperature, float humidity) {
  if (!s_espnow_initialized) return ESP_ERR_INVALID_STATE;
  
  espnow_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = ESPNOW_TYPE_TEMP;
  pkt.temp.temp_x10 = (int16_t)(temperature * 10);
  pkt.temp.humid_x10 = (int16_t)(humidity * 10);

  ESP_LOGI(TAG, "Bridging TEMP over ESP-NOW: %.1f°C, %.1f%%", temperature, humidity);
  esp_now_send(s_target_mac, (uint8_t *)&pkt, sizeof(pkt));

  for (int i = 0; i < s_peer_count; i++) {
    if (memcmp(s_peer_list[i], s_target_mac, 6) == 0) continue;
    esp_now_send(s_peer_list[i], (uint8_t *)&pkt, sizeof(pkt));
  }
  return ESP_OK;
}

esp_err_t svc_espnow_bridge_relay_send(uint8_t idx, bool state) {
  if (!s_espnow_initialized) return ESP_ERR_INVALID_STATE;

  espnow_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = ESPNOW_TYPE_RELAY;
  pkt.relay.idx = idx;
  pkt.relay.state = state;

  uint8_t *mac = s_relay_mac;
  ESP_LOGI(TAG, "Bridging RELAY %d update over ESP-NOW to " MACSTR, idx, MAC2STR(mac));
  esp_now_send(mac, (uint8_t *)&pkt, sizeof(pkt));

  for (int i = 0; i < s_peer_count; i++) {
    if (memcmp(s_peer_list[i], s_target_mac, 6) == 0) continue;
    esp_now_send(s_peer_list[i], (uint8_t *)&pkt, sizeof(pkt));
  }
  return ESP_OK;
}

int svc_espnow_scan_peers(espnow_discovered_device_t *devices, int max_devices) {
  s_discovered_count = 0;
  s_is_scanning = true;

  espnow_packet_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = ESPNOW_TYPE_DISCOVERY_REQ;

  uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peer_info = {0};
  memcpy(peer_info.peer_addr, broadcast_mac, 6);
  peer_info.channel = 0;
  peer_info.encrypt = false;

  peer_info.ifidx = WIFI_IF_STA;
  esp_now_add_peer(&peer_info);

  peer_info.ifidx = WIFI_IF_AP;
  esp_now_add_peer(&peer_info);

  ESP_LOGI(TAG, "Broadcasting ESP-NOW Discovery Request...");
  esp_now_send(broadcast_mac, (uint8_t *)&pkt, sizeof(pkt));

  // Wait 1500ms to gather responses
  vTaskDelay(pdMS_TO_TICKS(1500));

  s_is_scanning = false;

  int count = s_discovered_count;
  if (count > max_devices) count = max_devices;

  if (devices && count > 0) {
    memcpy(devices, s_discovered_devices, count * sizeof(espnow_discovered_device_t));
  }
  return count;
}


#else // If BRIDGE or SLAVE NOT enabled

esp_err_t svc_espnow_init(void) {
  return ESP_OK;
}

void svc_espnow_register_ac_handler(espnow_ac_handler_t handler) {}
void svc_espnow_register_led_handler(espnow_led_handler_t handler) {}
void svc_espnow_register_fan_handler(espnow_fan_handler_t handler) {}
void svc_espnow_register_temp_handler(espnow_temp_handler_t handler) {}
void svc_espnow_register_relay_handler(espnow_relay_handler_t handler) {}

esp_err_t svc_espnow_bridge_ac_send(const ir_ac_state_t *state, ac_brand_t brand, const char *custom_name) { return ESP_OK; }
esp_err_t svc_espnow_bridge_led_send(uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b, uint8_t speed) { return ESP_OK; }
esp_err_t svc_espnow_bridge_led_send_to(const uint8_t *mac, uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b, uint8_t speed) { return ESP_OK; }
esp_err_t svc_espnow_bridge_fan_send(const ir_fan_state_t *state, fan_brand_t brand, const char *custom_name) { return ESP_OK; }
esp_err_t svc_espnow_bridge_temp_send(float temperature, float humidity) { return ESP_OK; }
esp_err_t svc_espnow_bridge_relay_send(uint8_t idx, bool state) { return ESP_OK; }

esp_err_t svc_espnow_add_peer(const uint8_t *mac) { return ESP_OK; }
esp_err_t svc_espnow_remove_peer(const uint8_t *mac) { return ESP_OK; }
esp_err_t svc_espnow_get_peers(uint8_t *mac_list, int *count) { if (count) *count = 0; return ESP_OK; }
int svc_espnow_scan_peers(espnow_discovered_device_t *devices, int max_devices) { return 0; }

#endif // CONFIG_APP_ESPNOW_BRIDGE_ENABLE || CONFIG_APP_ESPNOW_SLAVE_ENABLE
