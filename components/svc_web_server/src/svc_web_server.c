#include "svc_web_server.h"
#include "cJSON.h"
#include "driver/temperature_sensor.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_vfs.h"
#include "mgr_ac_logic.h"
#include "mgr_fan_logic.h"
#include "mgr_ir_protocols.h"
#include "svc_espnow.h"
#include "svc_nvs.h"
#if CONFIG_APP_LED_CONTROL
#include "drv_led.h"
#endif
#include "int_rainmaker.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "svc_log.h"
#include "svc_ota.h"
#include "svc_wifi.h"
#include "sys_mem.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

static int hex_to_int(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

static void url_decode(char *dst, const char *src) {
  while (*src) {
    if (*src == '%' && src[1] && src[2]) {
      *dst++ = (char)(hex_to_int(src[1]) << 4 | hex_to_int(src[2]));
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

#define TAG "svc_web_server"
#define MOUNT_POINT "/www"

// Custom Brand Management
#define NVS_BRAND_KEY "custom_brands"
#define MAX_BRANDS 20
#define MAX_BRAND_NAME_LEN 32

static httpd_handle_t server = NULL;

static esp_err_t init_fs(void) {
  esp_vfs_spiffs_conf_t conf = {.base_path = MOUNT_POINT,
                                .partition_label = "storage",
                                .max_files = 5,
                                .format_if_mount_failed = true};
  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK) {
    return ret;
  }

  // Cleanup .tmp files to reclaim space
  DIR *dir = opendir(MOUNT_POINT);
  if (dir) {
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
      char full_path[512];
      snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, ent->d_name);
      
      if (strstr(ent->d_name, ".tmp")) {
        unlink(full_path);
        ESP_LOGI(TAG, "Cleaned up stale temp file: %s", full_path);
      } else {
        struct stat st;
        if (stat(full_path, &st) == 0) {
           ESP_LOGI(TAG, "File: %s (%ld bytes)", ent->d_name, st.st_size);
        }
      }
    }
    closedir(dir);
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS: Total: %d, Used: %d", total, used);
  }
  return ESP_OK;
}

/* -------------------------------------------------------------------------
 * Static File Handler
 * ------------------------------------------------------------------------- */
static esp_err_t static_file_handler(httpd_req_t *req) {
  char filepath[1280];
  // If root, serve index.html
  if (strcmp(req->uri, "/") == 0) {
    snprintf(filepath, sizeof(filepath), "%s/index.html", MOUNT_POINT);
  } else {
    snprintf(filepath, sizeof(filepath), "%s%s", MOUNT_POINT, req->uri);
  }

  // Identify MIME
  const char *mime = "text/plain";
  if (strstr(filepath, ".html"))
    mime = "text/html";
  else if (strstr(filepath, ".css"))
    mime = "text/css";
  else if (strstr(filepath, ".js"))
    mime = "application/javascript";
  else if (strstr(filepath, ".json"))
    mime = "application/json";
  else if (strstr(filepath, ".png"))
    mime = "image/png";
  else if (strstr(filepath, ".ico"))
    mime = "image/x-icon";

  FILE *f = fopen(filepath, "r");
  if (!f) {
    ESP_LOGE(TAG, "File not found: %s", filepath);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, mime);

  char chunk[1024];
  size_t chunksize;
  while ((chunksize = fread(chunk, 1, sizeof(chunk), f)) > 0) {
    if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
      fclose(f);
      return ESP_FAIL;
    }
  }
  fclose(f);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

/* -------------------------------------------------------------------------
 * API Handlers
 * ------------------------------------------------------------------------- */

static esp_err_t api_ac_control_handler(httpd_req_t *req) {
  char buf[128];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0)
    return ESP_FAIL;
  buf[ret] = 0;

  cJSON *json = cJSON_Parse(buf);
  if (!json)
    return ESP_FAIL;

  ir_ac_state_t state;
  mgr_ac_get_state(&state);

  if (cJSON_HasObjectItem(json, "power"))
    state.power = cJSON_GetObjectItem(json, "power")->valueint; // 0 or 1
  if (cJSON_HasObjectItem(json, "mode"))
    state.mode = cJSON_GetObjectItem(json, "mode")->valueint;
  if (cJSON_HasObjectItem(json, "temp"))
    state.temp = cJSON_GetObjectItem(json, "temp")->valueint;
  if (cJSON_HasObjectItem(json, "fan"))
    state.fan = cJSON_GetObjectItem(json, "fan")->valueint;

  // Handle brand (supports both preset and custom)
  if (cJSON_HasObjectItem(json, "is_custom") && cJSON_IsTrue(cJSON_GetObjectItem(json, "is_custom"))) {
      if (cJSON_HasObjectItem(json, "custom_brand_name")) {
          mgr_ac_set_custom_brand(cJSON_GetObjectItem(json, "custom_brand_name")->valuestring);
      }
  } else if (cJSON_HasObjectItem(json, "brand")) {
    cJSON *brand_item = cJSON_GetObjectItem(json, "brand");
    if (cJSON_IsNumber(brand_item)) {
      mgr_ac_set_brand((ac_brand_t)brand_item->valueint);
    } else if (cJSON_IsString(brand_item)) {
      mgr_ac_set_custom_brand(brand_item->valuestring);
    }
  }

  mgr_ac_set_state(&state);
  mgr_ac_send();
#if CONFIG_LAMP_PLATFORM_ANDROID
  app_rainmaker_update_ac(&state);
#endif

  cJSON_Delete(json);
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

static esp_err_t api_ac_state_handler(httpd_req_t *req) {
  ir_ac_state_t state;
  mgr_ac_get_state(&state);
  ac_brand_t brand = mgr_ac_get_brand();

  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "power", state.power);
  cJSON_AddNumberToObject(root, "mode", state.mode);
  cJSON_AddNumberToObject(root, "temp", state.temp);
  cJSON_AddNumberToObject(root, "fan", state.fan);
  cJSON_AddNumberToObject(root, "brand", (int)brand);

  char *str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, str, HTTPD_RESP_USE_STRLEN);

  cJSON_Delete(root);
  free(str);
  return ESP_OK;
}

/**
 * @brief Get full AC state including brand and custom brand info
 * This is used by WebUI to restore state on page load
 */
static esp_err_t api_ac_full_state_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();

  // Get brand info
  ac_brand_t brand = mgr_ac_get_brand();
  bool is_custom = mgr_ac_is_custom_brand();
  const char *custom_name = mgr_ac_get_custom_brand();

  cJSON_AddNumberToObject(root, "brand", (int)brand);
  cJSON_AddBoolToObject(root, "is_custom", is_custom);
  if (is_custom && custom_name) {
    cJSON_AddStringToObject(root, "custom_brand_name", custom_name);
  }

  // Get AC state
  ir_ac_state_t state;
  mgr_ac_get_state(&state);
  cJSON *state_obj = cJSON_CreateObject();
  cJSON_AddBoolToObject(state_obj, "power", state.power);
  cJSON_AddNumberToObject(state_obj, "mode", state.mode);
  cJSON_AddNumberToObject(state_obj, "temp", state.temp);
  cJSON_AddNumberToObject(state_obj, "fan", state.fan);
  cJSON_AddNumberToObject(state_obj, "swing_v", state.swing_v);
  cJSON_AddNumberToObject(state_obj, "swing_h", state.swing_h);
  cJSON_AddItemToObject(root, "state", state_obj);

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, json_str);

  free(json_str);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t api_fan_control_handler(httpd_req_t *req) {
  char buf[256];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0)
    return ESP_FAIL;
  buf[ret] = 0;

  cJSON *json = cJSON_Parse(buf);
  if (!json)
    return ESP_FAIL;

  ir_fan_state_t state;
  mgr_fan_get_state(&state);

  if (cJSON_HasObjectItem(json, "power"))
    state.power = cJSON_GetObjectItem(json, "power")->valueint;
  if (cJSON_HasObjectItem(json, "speed"))
    state.speed = cJSON_GetObjectItem(json, "speed")->valueint;
  if (cJSON_HasObjectItem(json, "swing"))
    state.swing = cJSON_GetObjectItem(json, "swing")->valueint;

  if (cJSON_HasObjectItem(json, "is_custom") && cJSON_IsTrue(cJSON_GetObjectItem(json, "is_custom"))) {
      if (cJSON_HasObjectItem(json, "custom_brand_name")) {
          mgr_fan_set_custom_brand(cJSON_GetObjectItem(json, "custom_brand_name")->valuestring);
      }
  } else if (cJSON_HasObjectItem(json, "brand")) {
    cJSON *brand_item = cJSON_GetObjectItem(json, "brand");
    if (cJSON_IsNumber(brand_item)) {
      mgr_fan_set_brand((fan_brand_t)brand_item->valueint);
    } else if (cJSON_IsString(brand_item)) {
      mgr_fan_set_custom_brand(brand_item->valuestring);
    }
  }

  mgr_fan_set_state(&state);
  mgr_fan_send();

  cJSON_Delete(json);
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

static esp_err_t api_fan_state_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();

  ir_fan_state_t state;
  mgr_fan_get_state(&state);
  fan_brand_t brand = mgr_fan_get_brand();
  bool is_custom = mgr_fan_is_custom_brand();
  const char *custom_name = mgr_fan_get_custom_brand();

  cJSON_AddBoolToObject(root, "power", state.power);
  cJSON_AddNumberToObject(root, "speed", state.speed);
  cJSON_AddBoolToObject(root, "swing", state.swing);
  cJSON_AddNumberToObject(root, "brand", (int)brand);
  cJSON_AddBoolToObject(root, "is_custom", is_custom);
  if (is_custom && custom_name) {
    cJSON_AddStringToObject(root, "custom_brand_name", custom_name);
  }

  char *str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, str);

  free(str);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t api_nodes_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    uint8_t macs[10][6];
    int count = 0;
    svc_espnow_get_peers((uint8_t *)macs, &count);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
      char mac_str[18];
      snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(macs[i]));
      cJSON_AddItemToArray(root, cJSON_CreateString(mac_str));
    }
    char *str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, str);
    free(str);
    cJSON_Delete(root);
    return ESP_OK;
  } else if (req->method == HTTP_POST) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
      return ESP_FAIL;
    buf[ret] = 0;

    cJSON *json = cJSON_Parse(buf);
    if (!json)
      return ESP_FAIL;

    if (cJSON_HasObjectItem(json, "mac")) {
      const char *mac_str = cJSON_GetObjectItem(json, "mac")->valuestring;
      uint8_t mac[6];
      int values[6];
      if (sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", &values[0],
                 &values[1], &values[2], &values[3], &values[4],
                 &values[5]) == 6) {
        for (int i = 0; i < 6; i++)
          mac[i] = (uint8_t)values[i];
        svc_espnow_add_peer(mac);
        httpd_resp_send(req, "OK", 2);
      } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid MAC");
      }
    }
    cJSON_Delete(json);
    return ESP_OK;
  } else if (req->method == HTTP_DELETE) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
      return ESP_FAIL;
    buf[ret] = 0;

    cJSON *json = cJSON_Parse(buf);
    if (!json)
      return ESP_FAIL;

    if (cJSON_HasObjectItem(json, "mac")) {
      const char *mac_str = cJSON_GetObjectItem(json, "mac")->valuestring;
      uint8_t mac[6];
      int values[6];
      if (sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", &values[0],
                 &values[1], &values[2], &values[3], &values[4],
                 &values[5]) == 6) {
        for (int i = 0; i < 6; i++)
          mac[i] = (uint8_t)values[i];
        svc_espnow_remove_peer(mac);
        httpd_resp_send(req, "OK", 2);
      } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid MAC");
      }
    }
    cJSON_Delete(json);
    return ESP_OK;
  }
  return ESP_FAIL;
}

static esp_err_t api_ir_list_handler(httpd_req_t *req) {
  cJSON *list = svc_nvs_get_ir_keys();
  char *json_str = cJSON_PrintUnformatted(list);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(list);
  free(json_str);
  return ESP_OK;
}

static esp_err_t api_learn_start_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API: Start Learn");
  esp_err_t err = mgr_ir_start_learn();
  if (err == ESP_OK) {
    httpd_resp_send(req, "Started", HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_learn_stop_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API: Stop Learn");
  mgr_ir_stop_learn();
  httpd_resp_send(req, "Stopped", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t api_learn_status_handler(httpd_req_t *req) {
  uint32_t count = 0;
  bool learning = mgr_ir_get_learn_status(&count);

  char resp[64];
  snprintf(resp, sizeof(resp), "{\"learning\":%s, \"captured\":%" PRIu32 "}",
           learning ? "true" : "false", count);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t api_matrix_save_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char dev_id_raw[64] = {0};
  char dev_id[32] = {0};
  char index_str[8] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "dev_id", dev_id_raw, sizeof(dev_id_raw)) == ESP_OK &&
          httpd_query_key_value(buf, "index", index_str, sizeof(index_str)) == ESP_OK) {
        
        url_decode(dev_id, dev_id_raw);
        int index = atoi(index_str);
        ESP_LOGI(TAG, "API: Matrix Save Dev=%s, Index=%d", dev_id, index);
        
        if (mgr_ir_save_to_matrix(dev_id, index) == ESP_OK) {
          httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
        } else {
          httpd_resp_send_500(req);
        }
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_save_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      char key_raw[32] = {0};
      if (httpd_query_key_value(buf, "key", key_raw, sizeof(key_raw)) == ESP_OK) {
        url_decode(key, key_raw);
        ESP_LOGI(TAG, "API: Save Key %s", key);
        if (mgr_ir_save_learned_result(key) == ESP_OK) {
          // Auto-Config: Update logic brand name based on the learned key prefix
          if (strncmp(key, "F_", 2) == 0) {
              char dev_name[32] = {0};
              char *underscore = strchr(key + 2, '_');
              if (underscore) {
                  memcpy(dev_name, key + 2, underscore - (key + 2));
                  mgr_fan_set_custom_brand(dev_name);
                  ESP_LOGI(TAG, "Auto-configured Fan brand to: %s", dev_name);
              }
          } else if (strncmp(key, "A_", 2) == 0) {
              char dev_name[32] = {0};
              char *underscore = strchr(key + 2, '_');
              if (underscore) {
                  memcpy(dev_name, key + 2, underscore - (key + 2));
                  mgr_ac_set_custom_brand(dev_name);
                  ESP_LOGI(TAG, "Auto-configured AC brand to: %s", dev_name);
              }
          }
          httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
        } else {
          httpd_resp_send_500(req);
        }
      }
    }
    free(buf);
  }

  if (key[0] == 0) {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_send_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      char key_raw[32] = {0};
      if (httpd_query_key_value(buf, "key", key_raw, sizeof(key_raw)) == ESP_OK) {
        url_decode(key, key_raw);
        ESP_LOGI(TAG, "API: Send Key %s", key);
        mgr_ir_send_key(key);
        httpd_resp_send(req, "Sent", HTTPD_RESP_USE_STRLEN);
      }
    }
    free(buf);
  }

  if (key[0] == 0) {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_delete_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      char key_raw[32] = {0};
      if (httpd_query_key_value(buf, "key", key_raw, sizeof(key_raw)) == ESP_OK) {
        url_decode(key, key_raw);
        ESP_LOGI(TAG, "API: Delete Key %s", key);
        svc_nvs_delete_ir(key);
        httpd_resp_send(req, "Deleted", HTTPD_RESP_USE_STRLEN);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_rename_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char old_key[32] = {0};
  char new_key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      httpd_query_key_value(buf, "old", old_key, sizeof(old_key));
      httpd_query_key_value(buf, "new", new_key, sizeof(new_key));

      if (old_key[0] && new_key[0]) {
        ESP_LOGI(TAG, "API: Rename %s -> %s", old_key, new_key);
        if (svc_nvs_rename_ir(old_key, new_key) == ESP_OK) {
          httpd_resp_send(req, "Renamed", HTTPD_RESP_USE_STRLEN);
        } else {
          httpd_resp_send_500(req);
        }
      } else {
        httpd_resp_send_404(req);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

#if CONFIG_APP_OTA_ENABLE
static esp_err_t api_ota_check_handler(httpd_req_t *req) {
  char remote_ver_str[32] = {0};
  char response[256];
  bool available = false;
  bool time_synced = true;

  // Check if time is synced (required for TLS certificate validation)
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  if (timeinfo.tm_year < (2020 - 1900)) {
    time_synced = false;
  }

  // Optimistic check. If logic fails or server is down, we handle it.
  // Return local cached status to avoid blocking the Web Server thread
  const char *cached = svc_ota_get_cached_version();
  available = svc_ota_is_update_available();
  strncpy(remote_ver_str, cached, sizeof(remote_ver_str) - 1);

  // Trigger a fresh background check
  svc_ota_trigger_check();

  snprintf(response, sizeof(response),
           "{\"current\":\"%s\", \"latest\":\"%s\", \"available\":%s, "
           "\"time_synced\":%s}",
           PROJECT_VERSION, remote_ver_str, available ? "true" : "false",
           time_synced ? "true" : "false");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t api_ota_start_handler(httpd_req_t *req) {
  char url[256];
  snprintf(url, sizeof(url), "%s/lamp-ir-device.bin", CONFIG_OTA_SERVER_URL);

  ESP_LOGI(TAG, "Starting manual update from UI: %s", url);

  // When triggering manually, we might not know the exact version unless
  // checking first. For now, label it as "Manual Update" or similar.
  if (svc_ota_start(url) == ESP_OK) {
    httpd_resp_send(req, "Started", HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}
#endif

static esp_err_t api_wifi_config_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char ssid[33] = {0};
  char password[65] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK) {
        httpd_query_key_value(buf, "password", password, sizeof(password));

        ESP_LOGI(TAG, "API: Update WiFi Config. SSID: %s", ssid);
        svc_wifi_update_credentials(ssid, password);
        httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
      } else {
        httpd_resp_send_500(req);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_wifi_scan_handler(httpd_req_t *req) {
  uint16_t ap_count = 0;
  wifi_ap_record_t *ap_list = NULL;

  if (svc_wifi_get_scan_results(&ap_list, &ap_count) != ESP_OK) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  cJSON *root = cJSON_CreateArray();
  for (int i = 0; i < ap_count; i++) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "ssid", (char *)ap_list[i].ssid);
    cJSON_AddNumberToObject(item, "rssi", ap_list[i].rssi);
    cJSON_AddItemToArray(root, item);
  }
  free(ap_list);

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

#if CONFIG_APP_LED_CONTROL
static esp_err_t api_led_config_get_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char param[32] = {0};
  drv_led_effect_t effect = DRV_LED_EFFECT_STATIC;

  // Check if specific effect requested
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "effect", param, sizeof(param)) ==
          ESP_OK) {
        if (strcmp(param, "rainbow") == 0)
          effect = DRV_LED_EFFECT_RAINBOW;
        else if (strcmp(param, "running") == 0)
          effect = DRV_LED_EFFECT_RUNNING;
        else if (strcmp(param, "breathing") == 0)
          effect = DRV_LED_EFFECT_BREATHING;
        else if (strcmp(param, "blink") == 0)
          effect = DRV_LED_EFFECT_BLINK;
        else if (strcmp(param, "knight_rider") == 0)
          effect = DRV_LED_EFFECT_KNIGHT_RIDER;
        else if (strcmp(param, "loading") == 0)
          effect = DRV_LED_EFFECT_LOADING;
        else if (strcmp(param, "color_wipe") == 0)
          effect = DRV_LED_EFFECT_COLOR_WIPE;
        else if (strcmp(param, "theater_chase") == 0)
          effect = DRV_LED_EFFECT_THEATER_CHASE;
        else if (strcmp(param, "fire") == 0)
          effect = DRV_LED_EFFECT_FIRE;
        else if (strcmp(param, "sparkle") == 0)
          effect = DRV_LED_EFFECT_SPARKLE;
        else
          effect = DRV_LED_EFFECT_STATIC;
      } else {
        // If no effect specified, get current
        uint8_t r, g, b, br, sp;
        drv_led_get_config(&r, &g, &b, &effect, &br, &sp);
      }
    }
    free(buf);
  } else {
    // If no query string, get current
    uint8_t r, g, b, br, sp;
    drv_led_get_config(&r, &g, &b, &effect, &br, &sp);
  }

  uint8_t speed = 50;
  uint8_t colors[CONFIG_APP_LED_COUNT][3];
  memset(colors, 0, sizeof(colors));
  drv_led_get_effect_config(effect, &speed, colors);

  // Get global brightness
  uint8_t global_br;
  drv_led_get_config(NULL, NULL, NULL, NULL, &global_br, NULL);

  // Construct JSON manually or via cJSON
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "speed", speed);
  cJSON_AddNumberToObject(root, "brightness", global_br);

  // Map enum to string
  const char *effect_str = "static";
  switch (effect) {
  case DRV_LED_EFFECT_RAINBOW:
    effect_str = "rainbow";
    break;
  case DRV_LED_EFFECT_RUNNING:
    effect_str = "running";
    break;
  case DRV_LED_EFFECT_BREATHING:
    effect_str = "breathing";
    break;
  case DRV_LED_EFFECT_BLINK:
    effect_str = "blink";
    break;
  case DRV_LED_EFFECT_KNIGHT_RIDER:
    effect_str = "knight_rider";
    break;
  case DRV_LED_EFFECT_LOADING:
    effect_str = "loading";
    break;
  case DRV_LED_EFFECT_COLOR_WIPE:
    effect_str = "color_wipe";
    break;
  case DRV_LED_EFFECT_THEATER_CHASE:
    effect_str = "theater_chase";
    break;
  case DRV_LED_EFFECT_FIRE:
    effect_str = "fire";
    break;
  case DRV_LED_EFFECT_SPARKLE:
    effect_str = "sparkle";
    break;
  default:
    effect_str = "static";
    break;
  }
  cJSON_AddStringToObject(root, "effect", effect_str);

  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < (CONFIG_APP_LED_COUNT < 8 ? CONFIG_APP_LED_COUNT : 8);
       i++) {
    cJSON *c = cJSON_CreateArray();
    cJSON_AddItemToArray(c, cJSON_CreateNumber(colors[i][0]));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(colors[i][1]));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(colors[i][2]));
    cJSON_AddItemToArray(arr, c);
  }
  cJSON_AddItemToObject(root, "colors", arr);

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

static esp_err_t api_led_config_post_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char param[32] = {0};
  uint8_t r = 0, g = 0, b = 0;
  int index = -1;
  drv_led_effect_t effect = DRV_LED_EFFECT_STATIC;
  bool effect_found = false;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {

      // Determine Effect
      if (httpd_query_key_value(buf, "effect", param, sizeof(param)) ==
          ESP_OK) {
        if (strcmp(param, "rainbow") == 0)
          effect = DRV_LED_EFFECT_RAINBOW;
        else if (strcmp(param, "running") == 0)
          effect = DRV_LED_EFFECT_RUNNING;
        else if (strcmp(param, "breathing") == 0)
          effect = DRV_LED_EFFECT_BREATHING;
        else if (strcmp(param, "blink") == 0)
          effect = DRV_LED_EFFECT_BLINK;
        else if (strcmp(param, "knight_rider") == 0)
          effect = DRV_LED_EFFECT_KNIGHT_RIDER;
        else if (strcmp(param, "loading") == 0)
          effect = DRV_LED_EFFECT_LOADING;
        else if (strcmp(param, "color_wipe") == 0)
          effect = DRV_LED_EFFECT_COLOR_WIPE;
        else if (strcmp(param, "theater_chase") == 0)
          effect = DRV_LED_EFFECT_THEATER_CHASE;
        else if (strcmp(param, "fire") == 0)
          effect = DRV_LED_EFFECT_FIRE;
        else if (strcmp(param, "sparkle") == 0)
          effect = DRV_LED_EFFECT_SPARKLE;
        else if (strcmp(param, "random") == 0)
          effect = DRV_LED_EFFECT_RANDOM;
        else if (strcmp(param, "auto_cycle") == 0)
          effect = DRV_LED_EFFECT_AUTO_CYCLE;
        else
          effect = DRV_LED_EFFECT_STATIC;
        effect_found = true;
      }

      // Brightness (Global)
      if (httpd_query_key_value(buf, "brightness", param, sizeof(param)) ==
          ESP_OK) {
        drv_led_set_brightness(atoi(param));
      }

      // Speed (Per Effect)
      if (effect_found) {
        drv_led_set_effect(effect);
      } else {
        // Get current effect to modify it
        uint8_t dummy1, dummy2, dummy3, dummy4, dummy5;
        drv_led_get_config(&dummy1, &dummy2, &dummy3, &effect, &dummy4,
                           &dummy5);
      }

      // Now set Speed for this effect
      if (httpd_query_key_value(buf, "speed", param, sizeof(param)) == ESP_OK) {
        drv_led_set_speed(atoi(param));
      }

      // Colors
      if (httpd_query_key_value(buf, "index", param, sizeof(param)) == ESP_OK) {
        index = atoi(param); // 0-7, or 255 for all
        if (index < 0)
          index = 255;
      }

      bool c_set = false;
      if (httpd_query_key_value(buf, "r", param, sizeof(param)) == ESP_OK) {
        r = atoi(param);
        c_set = true;
      }
      if (httpd_query_key_value(buf, "g", param, sizeof(param)) == ESP_OK) {
        g = atoi(param);
        c_set = true;
      }
      if (httpd_query_key_value(buf, "b", param, sizeof(param)) == ESP_OK) {
        b = atoi(param);
        c_set = true;
      }

      if (c_set && index != -1) {
        drv_led_set_config(effect, index, r, g, b);
      }

      httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
    } else {
      httpd_resp_send_500(req);
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_led_save_preset_handler(httpd_req_t *req) {
  if (drv_led_save_settings() == ESP_OK) {
    httpd_resp_send(req, "Saved", 5);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_led_state_config_get_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateArray();
  // Expose specific states user might want to configure
  drv_led_state_t states[] = {
      DRV_LED_WIFI_PROV, DRV_LED_WIFI_CONN, DRV_LED_OTA,       DRV_LED_IR_TX,
      DRV_LED_IR_LEARN,  DRV_LED_IR_FAIL,   DRV_LED_IR_SUCCESS};
  const char *names[] = {"WiFi Prov", "WiFi Conn", "OTA",       "IR TX",
                         "IR Learn",  "IR Fail",   "IR Success"};

  for (int i = 0; i < 7; i++) {
    uint8_t r, g, b;
    drv_led_get_state_color(states[i], &r, &g, &b);
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", states[i]);
    cJSON_AddStringToObject(item, "name", names[i]);
    cJSON_AddNumberToObject(item, "r", r);
    cJSON_AddNumberToObject(item, "g", g);
    cJSON_AddNumberToObject(item, "b", b);
    cJSON_AddItemToArray(root, item);
  }

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

static esp_err_t api_led_state_config_post_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char param[32] = {0};
  int id = -1;
  uint8_t r = 0, g = 0, b = 0;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "id", param, sizeof(param)) == ESP_OK)
        id = atoi(param);
      if (httpd_query_key_value(buf, "r", param, sizeof(param)) == ESP_OK)
        r = atoi(param);
      if (httpd_query_key_value(buf, "g", param, sizeof(param)) == ESP_OK)
        g = atoi(param);
      if (httpd_query_key_value(buf, "b", param, sizeof(param)) == ESP_OK)
        b = atoi(param);

      if (id >= 0) {
        drv_led_set_state_color((drv_led_state_t)id, r, g, b);
        httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
      } else {
        httpd_resp_send_500(req);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}
#endif

static esp_err_t api_system_logs_handler(httpd_req_t *req) {
  // Debug: Print to UART to confirm handler is called
  ESP_LOGI(TAG, "API: Fetch Logs");

  char *buf = malloc(4096);
  if (buf) {
    int len = svc_log_get_buffer(buf, 4096);
    if (len > 0) {
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_send(req, buf, len);
    } else {
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_send(req, "No logs available...", HTTPD_RESP_USE_STRLEN);
    }
    free(buf);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_system_logs_clear_handler(httpd_req_t *req) {
  svc_log_clear();
  httpd_resp_send(req, "Cleared", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

/* -------------------------------------------------------------------------
 * Custom Brand Management - NVS Helper Functions
 * ------------------------------------------------------------------------- */

// Helper: Load custom brands from NVS


// Helper: Check if brand exists in array
static bool brand_exists(cJSON *brands_array, const char *name) {
  cJSON *item = NULL;
  cJSON_ArrayForEach(item, brands_array) {
    cJSON *n = cJSON_GetObjectItem(item, "name");
    if (cJSON_IsString(n) && strcmp(n->valuestring, name) == 0) {
      return true;
    }
  }
  return false;
}

/* -------------------------------------------------------------------------
 * Custom Brand Management - API Handlers
 * ------------------------------------------------------------------------- */

// GET /api/brand/list
static void update_rainmaker_brands() {
  cJSON *brands = NULL;
  svc_nvs_load_custom_brands(&brands);

  size_t count = 0;
  char **brand_list = NULL;

  if (brands) {
    count = cJSON_GetArraySize(brands);
    if (count > 0) {
      brand_list = malloc(count * sizeof(char *));
      if (brand_list) {
        int idx = 0;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, brands) {
          cJSON *n = cJSON_GetObjectItem(item, "name");
          brand_list[idx] = n ? n->valuestring : "Unknown";
          idx++;
        }
      } else {
        count = 0;
      }
    }
  }

#if CONFIG_LAMP_PLATFORM_ANDROID
  app_rainmaker_update_custom_brands(brand_list, count);
#endif

  if (brand_list)
    free(brand_list);
  if (brands)
    cJSON_Delete(brands);
}

static esp_err_t api_brand_list_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API Brand List");

  cJSON *brands = NULL;
  svc_nvs_load_custom_brands(&brands);

  const char *json_str = cJSON_PrintUnformatted(brands);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

  cJSON_Delete(brands);
  free((void *)json_str);
  return ESP_OK;
}

// POST /api/brand/add?name={name}&type={type}
static esp_err_t api_brand_add_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API Brand Add");

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameters");
    return ESP_FAIL;
  }

  char name_raw[MAX_BRAND_NAME_LEN];
  char type_raw[32];
  char icon_raw[32] = {0};
  char name[MAX_BRAND_NAME_LEN];
  char type[32];
  char icon[32] = {0};
  
  if (httpd_query_key_value(query, "name", name_raw, sizeof(name_raw)) != ESP_OK ||
      httpd_query_key_value(query, "type", type_raw, sizeof(type_raw)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing name or type");
    return ESP_FAIL;
  }
  if (httpd_query_key_value(query, "icon", icon_raw, sizeof(icon_raw)) == ESP_OK) {
      url_decode(icon, icon_raw);
  }
  url_decode(name, name_raw);
  url_decode(type, type_raw);

  cJSON *brands = NULL;
  svc_nvs_load_custom_brands(&brands);

  if (cJSON_GetArraySize(brands) >= MAX_BRANDS) {
    cJSON_Delete(brands);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Max brands reached");
    return ESP_FAIL;
  }

  if (brand_exists(brands, name)) {
    cJSON_Delete(brands);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Brand already exists");
    return ESP_FAIL;
  }

  cJSON *new_brand = cJSON_CreateObject();
  cJSON_AddStringToObject(new_brand, "name", name);
  cJSON_AddStringToObject(new_brand, "type", type);
  if (icon[0]) {
      cJSON_AddStringToObject(new_brand, "icon", icon);
  }
  cJSON_AddItemToArray(brands, new_brand);
  
  svc_nvs_save_custom_brands(brands);
  update_rainmaker_brands();
  cJSON_Delete(brands);

  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// POST /api/brand/rename?old={old}&new={new}
static esp_err_t api_brand_rename_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API Brand Rename");

  char query[256];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameters");
    return ESP_FAIL;
  }

  char old_name_raw[MAX_BRAND_NAME_LEN], new_name_raw[MAX_BRAND_NAME_LEN];
  char old_name[MAX_BRAND_NAME_LEN], new_name[MAX_BRAND_NAME_LEN];
  if (httpd_query_key_value(query, "old", old_name_raw, sizeof(old_name_raw)) !=
          ESP_OK ||
      httpd_query_key_value(query, "new", new_name_raw, sizeof(new_name_raw)) !=
          ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameters");
    return ESP_FAIL;
  }
  url_decode(old_name, old_name_raw);
  url_decode(new_name, new_name_raw);


  cJSON *brands = NULL;
  svc_nvs_load_custom_brands(&brands);

  // Find and rename
  bool found = false;
  cJSON *item = NULL;
  cJSON_ArrayForEach(item, brands) {
    cJSON *n = cJSON_GetObjectItem(item, "name");
    if (cJSON_IsString(n) && strcmp(n->valuestring, old_name) == 0) {
      cJSON_ReplaceItemInObject(item, "name", cJSON_CreateString(new_name));
      found = true;
      break;
    }
  }

  if (!found) {
    cJSON_Delete(brands);
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Brand not found");
    return ESP_FAIL;
  }

  svc_nvs_save_custom_brands(brands);
  update_rainmaker_brands();
  cJSON_Delete(brands);

  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// POST /api/brand/delete?name={name}
static esp_err_t api_brand_delete_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API Brand Delete");

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing name parameter");
    return ESP_FAIL;
  }

  char name_raw[MAX_BRAND_NAME_LEN];
  char name[MAX_BRAND_NAME_LEN];
  if (httpd_query_key_value(query, "name", name_raw, sizeof(name_raw)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing name parameter");
    return ESP_FAIL;
  }
  url_decode(name, name_raw);


  cJSON *brands = NULL;
  svc_nvs_load_custom_brands(&brands);

  // Find and delete
  int index = 0;
  bool found = false;
  cJSON *item = NULL;
  cJSON_ArrayForEach(item, brands) {
    cJSON *n = cJSON_GetObjectItem(item, "name");
    if (cJSON_IsString(n) && strcmp(n->valuestring, name) == 0) {
      cJSON_DeleteItemFromArray(brands, index);
      found = true;
      break;
    }
    index++;
  }

  if (!found) {
    cJSON_Delete(brands);
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Brand not found");
    return ESP_FAIL;
  }

  svc_nvs_save_custom_brands(brands);
  update_rainmaker_brands();
  cJSON_Delete(brands);

  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}
static temperature_sensor_handle_t temp_sensor = NULL;

static esp_err_t api_system_stats_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API Stats Requested");
  cJSON *root = cJSON_CreateObject();
#if CONFIG_APP_LED_CONTROL
  cJSON_AddBoolToObject(root, "led_enabled", true);
#else
  cJSON_AddBoolToObject(root, "led_enabled", false);
#endif

  // Uptime
  int64_t uptime_us = esp_timer_get_time();
  cJSON_AddNumberToObject(root, "uptime", uptime_us / 1000000);

  // Heap / RAM
  multi_heap_info_t heap_info;
  heap_caps_get_info(&heap_info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t total_heap =
      heap_info.total_free_bytes + heap_info.total_allocated_bytes;
  size_t free_heap = heap_info.total_free_bytes;
  int ram_percent = total_heap > 0
                        ? ((heap_info.total_allocated_bytes * 100) / total_heap)
                        : 0;
  cJSON_AddNumberToObject(root, "free_heap", free_heap);
  cJSON_AddNumberToObject(root, "min_free_heap",
                          esp_get_minimum_free_heap_size());
  cJSON_AddNumberToObject(root, "ram", ram_percent);

  // PSRAM
  multi_heap_info_t psram_info;
  heap_caps_get_info(&psram_info, MALLOC_CAP_SPIRAM);
  size_t psram_total =
      psram_info.total_free_bytes + psram_info.total_allocated_bytes;
  int psram_percent =
      psram_total > 0 ? ((psram_info.total_allocated_bytes * 100) / psram_total)
                      : 0;
  cJSON_AddNumberToObject(root, "psram_free",
                          (double)psram_info.total_free_bytes);
  cJSON_AddNumberToObject(root, "psram_total", (double)psram_total);
  cJSON_AddNumberToObject(root, "psram", psram_percent);

  // CPU (placeholder - ESP32 doesn't have easy CPU usage)
  cJSON_AddNumberToObject(root, "cpu", 0);

  // Temperature (S3/C3 internal)
  float tsens_out = 0;
#if SOC_TEMP_SENSOR_SUPPORTED
  if (!temp_sensor) {
    temperature_sensor_config_t temp_sensor_config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) ==
        ESP_OK) {
      temperature_sensor_enable(temp_sensor);
    }
  }
  if (temp_sensor) {
    temperature_sensor_get_celsius(temp_sensor, &tsens_out);
  }
#endif
  cJSON_AddNumberToObject(root, "temp", tsens_out);

  // WiFi RSSI
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    cJSON_AddNumberToObject(root, "rssi", ap_info.rssi);
    cJSON_AddStringToObject(root, "ssid", (char *)ap_info.ssid);
  } else {
    cJSON_AddNumberToObject(root, "rssi", 0);
    cJSON_AddStringToObject(root, "ssid", "Disconnected");
  }

  // Version
#ifdef PROJECT_VERSION
  cJSON_AddStringToObject(root, "version", PROJECT_VERSION);
#else
  cJSON_AddStringToObject(root, "version", "1.0.0");
#endif

  const char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free((void *)json_str);
  return ESP_OK;
}

static esp_err_t api_system_restart_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "System Restart Requested");
  httpd_resp_send(req, "Restarting...", HTTPD_RESP_USE_STRLEN);
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
  return ESP_OK;
}


static esp_err_t api_weather_handler(httpd_req_t *req) {
  extern float svc_weather_get_temp(void);
  extern const char* svc_weather_get_desc(void);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "temp", svc_weather_get_temp());
  cJSON_AddStringToObject(root, "condition", svc_weather_get_desc());

  const char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free((void *)json_str);
  return ESP_OK;
}

static const httpd_uri_t api_stats = {.uri = "/api/stats",
                                       .method = HTTP_GET,
                                       .handler = api_system_stats_handler};
static const httpd_uri_t system_restart = {
    .uri = "/api/system/restart", .method = HTTP_POST, .handler = api_system_restart_handler};
static const httpd_uri_t ir_list = {
    .uri = "/api/ir/list", .method = HTTP_GET, .handler = api_ir_list_handler};
static const httpd_uri_t learn_start = {.uri = "/api/learn/start",
                                        .method = HTTP_POST,
                                        .handler = api_learn_start_handler};
static const httpd_uri_t learn_stop = {.uri = "/api/learn/stop",
                                       .method = HTTP_POST,
                                       .handler = api_learn_stop_handler};
static const httpd_uri_t save_key = {
    .uri = "/api/save", .method = HTTP_POST, .handler = api_save_handler};
static const httpd_uri_t send_key = {
    .uri = "/api/send", .method = HTTP_POST, .handler = api_send_handler};
static const httpd_uri_t delete_key = {.uri = "/api/ir/delete",
                                       .method = HTTP_POST,
                                       .handler = api_delete_handler};
static const httpd_uri_t rename_key = {.uri = "/api/ir/rename",
                                       .method = HTTP_POST,
                                       .handler = api_rename_handler};

static const httpd_uri_t wifi_config = {.uri = "/api/wifi/config",
                                        .method = HTTP_POST,
                                        .handler = api_wifi_config_handler};
static const httpd_uri_t wifi_scan = {.uri = "/api/wifi/scan",
                                      .method = HTTP_GET,
                                      .handler = api_wifi_scan_handler};
#if CONFIG_APP_LED_CONTROL
static const httpd_uri_t led_config_get = {.uri = "/api/led/config",
                                           .method = HTTP_GET,
                                           .handler =
                                               api_led_config_get_handler};
static const httpd_uri_t led_config_post = {.uri = "/api/led/config",
                                            .method = HTTP_POST,
                                            .handler =
                                                api_led_config_post_handler};
static const httpd_uri_t led_state_get = {.uri = "/api/led/state-config",
                                          .method = HTTP_GET,
                                          .handler =
                                              api_led_state_config_get_handler};
static const httpd_uri_t led_state_post = {
    .uri = "/api/led/state-config",
    .method = HTTP_POST,
    .handler = api_led_state_config_post_handler};
#endif

static const httpd_uri_t system_logs = {.uri = "/api/system/logs",
                                        .method = HTTP_GET,
                                        .handler = api_system_logs_handler};
static const httpd_uri_t system_logs_clear = {
    .uri = "/api/system/logs/clear",
    .method = HTTP_POST,
    .handler = api_system_logs_clear_handler};

static const httpd_uri_t system_stats = {.uri = "/api/system/stats",
                                         .method = HTTP_GET,
                                         .handler = api_system_stats_handler};

static const httpd_uri_t ac_control = {.uri = "/api/ac/control",
                                       .method = HTTP_POST,
                                       .handler = api_ac_control_handler};
static const httpd_uri_t ac_state = {.uri = "/api/ac/state",
                                     .method = HTTP_GET,
                                     .handler = api_ac_state_handler};

esp_err_t svc_web_init(void) {
  init_fs();                 // Initialize SPIFFS
  update_rainmaker_brands(); // Sync brands on boot
  return ESP_OK;
}

bool app_web_is_running(void) { return (server != NULL); }

void svc_web_stop(void) {
  if (server) {
    ESP_LOGI(TAG, "Stopping Web Server...");
    httpd_stop(server);
    server = NULL;
  }
}

esp_err_t svc_web_start(void) {
  if (server) {
    ESP_LOGW(TAG, "Web Server already running");
    return ESP_OK;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  // Limits are now managed via sdkconfig (CONFIG_HTTPD_MAX_URI_HANDLERS)
  config.stack_size = 10240;
  config.server_port = 8080; // Use 8080 to avoid conflict with HomeKit HAP (80)
  config.uri_match_fn = httpd_uri_match_wildcard; // Enable wildcard matching
  config.max_uri_handlers = 60;

  ESP_LOGI(TAG, "Starting HTTP Server...");
  if (httpd_start(&server, &config) == ESP_OK) {
    int ret;
#define REG_URI(h)                                                             \
  if ((ret = httpd_register_uri_handler(server, h)) != ESP_OK)                 \
  ESP_LOGE(TAG, "Fail reg %s: %d", (h)->uri, ret)

    // Specific API Handlers (Exact match first)
    REG_URI(&api_stats);
    REG_URI(&system_stats);
    REG_URI(&system_restart);
    REG_URI(&ir_list);
    REG_URI(&learn_start);
    REG_URI(&learn_stop);

    httpd_uri_t learn_status = {.uri = "/api/learn/status",
                                .method = HTTP_GET,
                                .handler = api_learn_status_handler,
                                .user_ctx = NULL};
    REG_URI(&learn_status);

    REG_URI(&save_key);
    
    httpd_uri_t matrix_save = {.uri = "/api/matrix/save",
                                .method = HTTP_POST,
                                .handler = api_matrix_save_handler};
    REG_URI(&matrix_save);

    REG_URI(&send_key);
    REG_URI(&delete_key);
    REG_URI(&rename_key);

#if CONFIG_APP_OTA_ENABLE
    // OTA
    httpd_uri_t ota_check_uri = {.uri = "/api/ota/check",
                                 .method = HTTP_GET,
                                 .handler = api_ota_check_handler};
    REG_URI(&ota_check_uri);
    httpd_uri_t ota_start_uri = {.uri = "/api/ota/start",
                                 .method = HTTP_POST,
                                 .handler = api_ota_start_handler};
    REG_URI(&ota_start_uri);
#endif

    REG_URI(&wifi_config);
    REG_URI(&wifi_scan);
#if CONFIG_APP_LED_CONTROL
    REG_URI(&led_config_get);
    REG_URI(&led_config_post);

    httpd_uri_t led_save_preset = {.uri = "/api/led/save_preset",
                                   .method = HTTP_POST,
                                   .handler = api_led_save_preset_handler};
    REG_URI(&led_save_preset);

    REG_URI(&led_state_get);
    REG_URI(&led_state_post);
#endif
    REG_URI(&ac_control);
    REG_URI(&ac_state);

    // Full state endpoint (includes brand info)
    static const httpd_uri_t ac_full_state = {.uri = "/api/ac/full_state",
                                              .method = HTTP_GET,
                                              .handler =
                                                  api_ac_full_state_handler};
    REG_URI(&ac_full_state);

    REG_URI(&system_logs);
    REG_URI(&system_logs_clear);

    static const httpd_uri_t brand_list_uri = {.uri = "/api/brand/list",
                                               .method = HTTP_GET,
                                               .handler =
                                                   api_brand_list_handler};
    REG_URI(&brand_list_uri);

    static const httpd_uri_t weather_uri = {.uri = "/api/weather",
                                            .method = HTTP_GET,
                                            .handler = api_weather_handler};
    REG_URI(&weather_uri);

    static const httpd_uri_t brand_add_uri = {.uri = "/api/brand/add",
                                              .method = HTTP_POST,
                                              .handler = api_brand_add_handler};
    REG_URI(&brand_add_uri);

    static const httpd_uri_t brand_rename_uri = {.uri = "/api/brand/rename",
                                                 .method = HTTP_POST,
                                                 .handler =
                                                     api_brand_rename_handler};
    REG_URI(&brand_rename_uri);

    static const httpd_uri_t brand_delete_uri = {.uri = "/api/brand/delete",
                                                 .method = HTTP_POST,
                                                 .handler =
                                                     api_brand_delete_handler};
    REG_URI(&brand_delete_uri);

    // Fan Handlers
    static const httpd_uri_t fan_control = {.uri = "/api/fan/control",
                                            .method = HTTP_POST,
                                            .handler = api_fan_control_handler};
    REG_URI(&fan_control);
    static const httpd_uri_t fan_state = {.uri = "/api/fan/state",
                                          .method = HTTP_GET,
                                          .handler = api_fan_state_handler};
    REG_URI(&fan_state);

    // Node Handlers (Multi-Node ESP-NOW)
    static const httpd_uri_t nodes_get = {
        .uri = "/api/nodes", .method = HTTP_GET, .handler = api_nodes_handler};
    REG_URI(&nodes_get);
    static const httpd_uri_t nodes_post = {
        .uri = "/api/nodes", .method = HTTP_POST, .handler = api_nodes_handler};
    REG_URI(&nodes_post);
    static const httpd_uri_t nodes_delete = {.uri = "/api/nodes",
                                             .method = HTTP_DELETE,
                                             .handler = api_nodes_handler};
    REG_URI(&nodes_delete);

    // Static File Handler (Wildcard catch-all at the end)
    static const httpd_uri_t static_files = {
        .uri = "/*", // Wildcard to catch everything else
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = NULL};
    REG_URI(&static_files);

    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "Error starting server!");
    return ESP_FAIL;
  }
}
