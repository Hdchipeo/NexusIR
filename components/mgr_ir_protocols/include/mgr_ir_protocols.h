/**
 * @file mgr_ir_protocols.h
 * @brief High-level IR Application Layer & Protocol Manager (Phase 3 - Migration & Export/Import)
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "driver/rmt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IR Command types
 */
typedef enum {
  APP_IR_CMD_AC_ON,
  APP_IR_CMD_AC_OFF,
  APP_IR_CMD_TEMP_UP,
  APP_IR_CMD_MAX
} mgr_ir_cmd_t;

/**
 * @brief IR Session States
 */
typedef enum {
  IR_STATE_IDLE = 0,
  IR_STATE_ARMED,        /**< Actively listening for IR signals */
  IR_STATE_CAPTURED,     /**< Valid signal received and held in buffer */
  IR_STATE_TIMEOUT,      /**< Session expired without receiving valid signal */
  IR_STATE_CANCELLED,    /**< Session explicitly stopped/cancelled */
  IR_STATE_ERROR         /**< Internal error or hardware failure */
} ir_session_state_t;

/**
 * @brief IR Session Information Snapshot
 */
typedef struct {
  uint32_t session_id;         /**< Monotonically increasing session ID */
  ir_session_state_t state;    /**< Current operational state */
  uint32_t symbol_count;       /**< Number of symbols captured */
  bool is_truncated;           /**< True if buffer limit was reached */
} mgr_ir_session_info_t;

/**
 * @brief Initialize IR Application and worker task
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_init(void);

/**
 * @brief IR RX Callback type
 * @param durations Array of pulse/space durations in microseconds
 * @param count Number of durations
 */
typedef void (*mgr_ir_rx_cb_t)(const uint16_t *durations, size_t count);

/**
 * @brief Set the IR RX callback
 * @param cb Callback function
 */
void mgr_ir_set_rx_callback(mgr_ir_rx_cb_t cb);

/**
 * @brief Start IR Slave mode (Background listening)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_start_slave(void);

// --- RX / Learn APIs ---

/**
 * @brief Start IR Learning mode with default 15-second timeout
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_start_learn(void);

/**
 * @brief Start IR Learning mode with custom timeout
 * @param timeout_ms Timeout in milliseconds (e.g. 15000)
 * @param[out] out_session_id Optional pointer to receive allocated session ID
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_start_learn_with_timeout(uint32_t timeout_ms, uint32_t *out_session_id);

/**
 * @brief Stop / Cancel IR Learning mode
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_stop_learn(void);

/**
 * @brief Alias for mgr_ir_stop_learn
 */
esp_err_t mgr_ir_cancel_learn(void);

/**
 * @brief Get the current learning status and captured symbol count
 * @param[out] count Pointer to store symbol count
 * @return true if currently learning (ARMED), false otherwise
 */
bool mgr_ir_get_learn_status(uint32_t *count);

/**
 * @brief Get rich snapshot of the current IR session
 * @param[out] out_info Pointer to store session information
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_get_session_info(mgr_ir_session_info_t *out_info);

/**
 * @brief Safely copy captured symbols from internal buffer with mutex protection.
 * @param dst Destination buffer
 * @param max_words Capacity in rmt_symbol_word_t
 * @param out_words Stored actual word count
 * @param out_is_truncated True if capture reached MAX limit
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_copy_learning_buffer(rmt_symbol_word_t *dst, size_t max_words,
                                      size_t *out_words, bool *out_is_truncated);

/**
 * @brief Save the learned IR signal to NVS with V1 binary format and CRC32
 * @param key Key to save data under
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_save_learned_result(const char *key);

/**
 * @brief Save learned IR signal with custom repeat policy
 * @param key Key to save under
 * @param repeat_count Number of repeats (0 = 1 transmission)
 * @param repeat_gap_ms Gap between repeats
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_save_learned_with_params(const char *key, uint8_t repeat_count, uint16_t repeat_gap_ms);

// --- TX APIs ---

/**
 * @brief Send a predefined IR command
 * @param cmd Command to send
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_send_cmd(mgr_ir_cmd_t cmd);

/**
 * @brief Send a raw IR signal by key from NVS (Supports V1, Legacy RAW, Legacy Matrix)
 * Automatically triggers safe on-access migration if legacy format is detected.
 * @param key Key of the stored signal
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_send_key(const char *key);

/**
 * @brief Send raw IR signal durations (pulse/space in microseconds)
 * Compatible with IRremoteESP8266 Raw Data.
 * @param durations Array of durations in microseconds
 * @param count Number of elements in durations array
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_send_raw(const uint16_t *durations, size_t count);

/**
 * @brief Check if an IR key exists in NVS
 * @param prefix Key prefix (e.g., "A_" or "F_")
 * @param brand Brand name
 * @param suffix Key suffix (e.g., "ON", "OFF")
 * @return true if key exists
 */
bool mgr_ir_send_key_exists(const char *prefix, const char *brand, const char *suffix);

// --- Matrix APIs ---

/**
 * @brief Save the captured IR signal to a Matrix entry in NVS (V1 format with CRC32)
 * @param dev_id Device identifier (used for key: M_<DEV>_<INDEX>)
 * @param index Index in the matrix (0=Off, 1=16C, ..., 15=30C)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_save_to_matrix(const char *dev_id, int index);

/**
 * @brief Send an IR signal from a Matrix entry in NVS
 * @param dev_id Device identifier
 * @param index Index in the matrix
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_send_from_matrix(const char *dev_id, int index);

/**
 * @brief Check if a Matrix file exists for a device
 * @param dev_id Device identifier
 * @return true if exists
 */
bool mgr_ir_matrix_exists(const char *dev_id);

// --- Migration & Import/Export APIs (IR-13, IR-14) ---

/**
 * @brief Scans all keys in NVS namespace 'ir_data' and migrates legacy records to V1 with CRC32.
 * Safe against sudden power loss: writes and verifies each key atomically before proceeding.
 * @param[out] out_migrated Number of keys upgraded to V1
 * @param[out] out_skipped Number of keys already in valid V1 format
 * @param[out] out_errors Number of corrupt or unrecoverable keys encountered
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_migrate_all(size_t *out_migrated, size_t *out_skipped, size_t *out_errors);

/**
 * @brief Export an IR key as formatted JSON string
 * @param key Key name to export
 * @param[out] out_json Allocated JSON string (caller must free)
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND, ESP_ERR_NO_MEM
 */
esp_err_t mgr_ir_export_key_json(const char *key, char **out_json);

/**
 * @brief Import and validate an IR key from JSON string and save as V1 record in NVS
 * @param key Key name to store under
 * @param json_str JSON string containing durations and metadata
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG
 */
esp_err_t mgr_ir_import_key_json(const char *key, const char *json_str);

#ifdef __cplusplus
}
#endif
