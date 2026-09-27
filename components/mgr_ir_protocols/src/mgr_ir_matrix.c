/**
 * @file mgr_ir_matrix.c
 * @brief IR AC Matrix Management Layer (Unified with IR Core V1)
 */

#include "mgr_ir_protocols.h"
#include "mgr_ir_waveform.h"
#include "esp_log.h"
#include "driver/rmt_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "svc_nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "mgr_ir_matrix"
#define MAX_MATRIX_ENTRIES 16
#define MAX_IR_SYMBOLS 600

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

static void init_mutex(void) {
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
    if (!dev_id || strlen(dev_id) == 0 || index < 0 || index >= MAX_MATRIX_ENTRIES) {
        return ESP_ERR_INVALID_ARG;
    }

    init_mutex();
    if (xSemaphoreTake(s_matrix_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // 1. Safely snapshot captured symbols from IR Core
    rmt_symbol_word_t *raw_buf = (rmt_symbol_word_t *)malloc(MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t));
    if (!raw_buf) {
        xSemaphoreGive(s_matrix_mutex);
        return ESP_ERR_NO_MEM;
    }

    size_t captured_words = 0;
    bool is_truncated = false;
    esp_err_t err = mgr_ir_copy_learning_buffer(raw_buf, MAX_IR_SYMBOLS, &captured_words, &is_truncated);
    if (err != ESP_OK || captured_words == 0) {
        ESP_LOGW(TAG, "No valid captured symbols to save to matrix");
        free(raw_buf);
        xSemaphoreGive(s_matrix_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    // 2. Normalize waveform (strip leading space, filter artifacts, trim trailing zero)
    rmt_symbol_word_t *clean_buf = (rmt_symbol_word_t *)malloc(MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t));
    if (!clean_buf) {
        free(raw_buf);
        xSemaphoreGive(s_matrix_mutex);
        return ESP_ERR_NO_MEM;
    }

    size_t clean_words = 0;
    size_t clean_durations = 0;
    err = ir_waveform_normalize(raw_buf, captured_words, clean_buf, MAX_IR_SYMBOLS,
                                &clean_words, &clean_durations);
    free(raw_buf);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Waveform normalization failed: %s", esp_err_to_name(err));
        free(clean_buf);
        xSemaphoreGive(s_matrix_mutex);
        return err;
    }

    // 3. Serialize into V1 format with CRC32
    // AC commands should NOT repeat (repeat_count = 0)
    uint8_t flags = is_truncated ? IR_FLAG_TRUNCATED : IR_FLAG_NONE;
    uint8_t *v1_record = NULL;
    size_t record_size = 0;

    err = ir_waveform_serialize_v1(clean_buf, clean_durations, 0, 40, flags, &v1_record, &record_size);
    free(clean_buf);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Waveform serialization failed: %s", esp_err_to_name(err));
        xSemaphoreGive(s_matrix_mutex);
        return err;
    }

    // 4. Save to NVS
    char key[16];
    get_nvs_key(key, sizeof(key), dev_id, index);

    ESP_LOGI(TAG, "Saving Matrix entry [%s] (Index %d): %d durations, %d bytes (Truncated=%s)",
             key, index, (int)clean_durations, (int)record_size, is_truncated ? "YES" : "NO");

    err = svc_nvs_save_ir(key, v1_record, record_size);
    free(v1_record);
    xSemaphoreGive(s_matrix_mutex);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Matrix entry %s saved successfully", key);
    } else {
        ESP_LOGE(TAG, "Failed to save matrix entry %s to NVS: %s", key, esp_err_to_name(err));
    }
    return err;
}

esp_err_t mgr_ir_send_from_matrix(const char *dev_id, int index) {
    if (!dev_id || strlen(dev_id) == 0 || index < 0 || index >= MAX_MATRIX_ENTRIES) {
        return ESP_ERR_INVALID_ARG;
    }

    char key[16];
    get_nvs_key(key, sizeof(key), dev_id, index);

    ESP_LOGI(TAG, "Sending Matrix command: dev=%s index=%d (key=%s)", dev_id, index, key);

    // Reuse the unified transmission pipeline in mgr_ir_send_key
    // This automatically handles V1 CRC32 verification and legacy fallback!
    return mgr_ir_send_key(key);
}

bool mgr_ir_matrix_exists(const char *dev_id) {
    if (!dev_id || strlen(dev_id) == 0) return false;

    init_mutex();
    if (xSemaphoreTake(s_matrix_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    // Check index 0 (Off), and common indices like 1 (16C) and 9 (24C)
    const int check_indices[] = {0, 1, 9};
    bool exists = false;

    for (int i = 0; i < (int)(sizeof(check_indices) / sizeof(check_indices[0])); i++) {
        char key[16];
        get_nvs_key(key, sizeof(key), dev_id, check_indices[i]);
        size_t data_bytes = 0;
        esp_err_t ret = svc_nvs_load_ir(key, NULL, &data_bytes);
        if (ret == ESP_OK && data_bytes > 0) {
            exists = true;
            break;
        }
    }

    xSemaphoreGive(s_matrix_mutex);
    return exists;
}
