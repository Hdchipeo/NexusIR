#include "mgr_ir_protocols.h"
#include "esp_log.h"
#include "driver/rmt_types.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "svc_nvs.h"

#define TAG "mgr_ir_matrix"
#define MAX_MATRIX_ENTRIES 16

extern rmt_symbol_word_t *s_learning_symbols;
extern uint32_t s_learning_num_symbols;
static SemaphoreHandle_t s_matrix_mutex = NULL;

static void sanitize_dev_name(char *dst, const char *src, size_t max_len) {
    size_t idx = 0;
    for (int i = 0; src[i] && idx < max_len - 1; i++) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            if (c >= 'a' && c <= 'z') c = c - 32; // Uppercase
            dst[idx++] = c;
        }
    }
    dst[idx] = '\0';
}

static void init_mutex() {
    if (s_matrix_mutex == NULL) {
        s_matrix_mutex = xSemaphoreCreateMutex();
    }
}

static void get_nvs_key(char *key, size_t max_len, const char *dev_id, int index) {
    char safe_name[10] = {0}; // Max 9 chars for safety (NVS max 15)
    sanitize_dev_name(safe_name, dev_id, sizeof(safe_name));
    snprintf(key, max_len, "M_%s_%d", safe_name, index);
}

esp_err_t mgr_ir_save_to_matrix(const char *dev_id, int index) {
    if (index < 0 || index >= MAX_MATRIX_ENTRIES) return ESP_ERR_INVALID_ARG;
    if (!s_learning_symbols || s_learning_num_symbols == 0) return ESP_ERR_INVALID_STATE;

    init_mutex();
    if (xSemaphoreTake(s_matrix_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    char key[16];
    get_nvs_key(key, sizeof(key), dev_id, index);

    uint32_t num_symbols = s_learning_num_symbols * 2;
    size_t data_bytes = num_symbols * sizeof(uint16_t);

    ESP_LOGI(TAG, "Saving to NVS matrix key: %s (%d bytes)", key, (int)data_bytes);

    esp_err_t ret = svc_nvs_save_ir(key, s_learning_symbols, data_bytes);

    xSemaphoreGive(s_matrix_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Matrix entry %d saved", index);
    } else {
        ESP_LOGE(TAG, "Failed to save matrix entry %d to NVS", index);
    }
    return ret;
}

esp_err_t mgr_ir_send_from_matrix(const char *dev_id, int index) {
    if (index < 0 || index >= MAX_MATRIX_ENTRIES) return ESP_ERR_INVALID_ARG;

    init_mutex();
    if (xSemaphoreTake(s_matrix_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    char key[16];
    get_nvs_key(key, sizeof(key), dev_id, index);

    size_t data_bytes = 0;
    esp_err_t ret = svc_nvs_load_ir(key, NULL, &data_bytes);
    if (ret != ESP_OK || data_bytes == 0) {
        xSemaphoreGive(s_matrix_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t *durations = (uint16_t*)malloc(data_bytes);
    if (!durations) {
        xSemaphoreGive(s_matrix_mutex);
        return ESP_ERR_NO_MEM;
    }

    ret = svc_nvs_load_ir(key, durations, &data_bytes);
    xSemaphoreGive(s_matrix_mutex);

    if (ret != ESP_OK) {
        free(durations);
        return ret;
    }

    size_t num_symbols = data_bytes / sizeof(uint16_t);
    size_t word_count = (num_symbols + 1) / 2;

    ESP_LOGI(TAG, "Sending from NVS matrix key: %s (%d symbols)", key, (int)num_symbols);

    extern esp_err_t ir_engine_send_raw(const void *symbols, size_t count);
    ir_engine_send_raw(durations, word_count);

    free(durations);
    return ESP_OK;
}

bool mgr_ir_matrix_exists(const char *dev_id) {
    init_mutex();
    if (xSemaphoreTake(s_matrix_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    char key[16];
    get_nvs_key(key, sizeof(key), dev_id, 0); // Check if index 0 (Power Off) exists

    size_t data_bytes = 0;
    esp_err_t ret = svc_nvs_load_ir(key, NULL, &data_bytes);
    
    xSemaphoreGive(s_matrix_mutex);
    return (ret == ESP_OK && data_bytes > 0);
}
