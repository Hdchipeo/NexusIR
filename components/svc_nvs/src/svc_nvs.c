#include "svc_nvs.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <malloc.h>
#include <string.h>

#define TAG "svc_nvs"
#define NVS_NAMESPACE "ir_data"

esp_err_t svc_nvs_init(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  return err;
}

esp_err_t svc_nvs_save_ir(const char *key, const void *data, size_t len) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS Open for Write failed: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_set_blob(my_handle, key, data, len);
  if (err == ESP_OK) {
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "NVS Commit failed: %s", esp_err_to_name(err));
  } else {
    ESP_LOGE(TAG, "NVS Set Blob failed: %s", esp_err_to_name(err));
  }
  nvs_close(my_handle);
  return err;
}

esp_err_t svc_nvs_load_ir(const char *key, void *data, size_t *len) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS Open for Read failed: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_get_blob(my_handle, key, data, len);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "NVS Get Blob '%s' failed: %s", key, esp_err_to_name(err));
  }
  nvs_close(my_handle);
  return err;
}

esp_err_t svc_nvs_delete_ir(const char *key) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS Open for Delete failed: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_erase_key(my_handle, key);
  if (err == ESP_OK) {
    err = nvs_commit(my_handle);
  }
  nvs_close(my_handle);
  return err;
}

esp_err_t svc_nvs_rename_ir(const char *old_key, const char *new_key) {
  size_t len = 0;
  // Check if old key exists
  esp_err_t err = svc_nvs_load_ir(old_key, NULL, &len);
  if (err != ESP_OK)
    return err;

  // Load old data
  void *data = malloc(len);
  if (!data)
    return ESP_ERR_NO_MEM;

  svc_nvs_load_ir(old_key, data, &len);

  // Save with new key
  err = svc_nvs_save_ir(new_key, data, len);
  free(data);

  if (err != ESP_OK)
    return err;

  // Delete old key
  return svc_nvs_delete_ir(old_key);
}

cJSON *svc_nvs_get_ir_keys(void) {
  cJSON *list = cJSON_CreateArray();
  nvs_iterator_t it = NULL;
  esp_err_t res =
      nvs_entry_find(NVS_DEFAULT_PART_NAME, NVS_NAMESPACE, NVS_TYPE_BLOB, &it);
  while (res == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    cJSON_AddItemToArray(list, cJSON_CreateString(info.key));
    res = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  return list;
}

esp_err_t svc_nvs_load_custom_brands(cJSON **brands_array) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
  if (err != ESP_OK) {
    *brands_array = cJSON_CreateArray();
    return ESP_OK; // No brands yet
  }

  size_t required_size = 0;
  err = nvs_get_str(nvs, "custom_brands", NULL, &required_size);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(nvs);
    *brands_array = cJSON_CreateArray();
    return ESP_OK;
  }

  char *json_str = malloc(required_size);
  if (!json_str) {
    nvs_close(nvs);
    *brands_array = cJSON_CreateArray();
    return ESP_ERR_NO_MEM;
  }

  nvs_get_str(nvs, "custom_brands", json_str, &required_size);
  nvs_close(nvs);

  *brands_array = cJSON_Parse(json_str);
  free(json_str);

  if (*brands_array == NULL) {
    *brands_array = cJSON_CreateArray();
  }

  return ESP_OK;
}

esp_err_t svc_nvs_save_custom_brands(cJSON *brands_array) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
  if (err != ESP_OK)
    return err;

  char *json_str = cJSON_PrintUnformatted(brands_array);
  if (!json_str) {
    nvs_close(nvs);
    return ESP_ERR_NO_MEM;
  }

  err = nvs_set_str(nvs, "custom_brands", json_str);
  free(json_str);

  if (err == ESP_OK) {
    nvs_commit(nvs);
  }
  nvs_close(nvs);
  return err;
}
