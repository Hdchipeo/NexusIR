/**
 * @file mgr_ir_protocols.h
 * @brief High-level IR Application Layer
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief IR Command types
 */
typedef enum {
  APP_IR_CMD_AC_ON,
  APP_IR_CMD_AC_OFF,
  APP_IR_CMD_TEMP_UP,
  APP_IR_CMD_MAX
} mgr_ir_cmd_t;

// Note: mgr_ir_send_ac_state removed. Use mgr_ir_send_from_matrix instead.

/**
 * @brief Initialize IR Application
 *
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
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_start_slave(void);

// RX / Learn

/**
 * @brief Start IR Learning mode
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_start_learn(void);

/**
 * @brief Stop IR Learning mode
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_stop_learn(void);

/**
 * @brief Get the current learning status and captured symbol count
 * @param[out] count Pointer to store symbol count
 * @return true if currently learning, false otherwise
 */
bool mgr_ir_get_learn_status(uint32_t *count);

/**
 * @brief Save the learned IR signal to NVS
 *
 * @param key Key to save data under
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_save_learned_result(const char *key);

// TX

/**
 * @brief Send a predefined IR command
 *
 * @param cmd Command to send
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_send_cmd(mgr_ir_cmd_t cmd);

/**
 * @brief Send a raw IR signal by key (from NVS)
 *
 * @param key Key of the stored signal
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ir_send_key(const char *key);

/**
 * @brief Send raw IR signal durations (pulse/space in microseconds)
 * Compatible with IRremoteESP8266 Raw Data.
 *
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
/**
 * @brief Save the captured IR signal to a Matrix file (SPIFFS)
 * 
 * @param dev_id Device identifier (used for filename)
 * @param index Index in the matrix (0=Off, 1=16C, ..., 15=30C)
 * @return esp_err_t 
 */
esp_err_t mgr_ir_save_to_matrix(const char *dev_id, int index);

/**
 * @brief Send an IR signal from a Matrix file
 * 
 * @param dev_id Device identifier
 * @param index Index in the matrix
 * @return esp_err_t 
 */
esp_err_t mgr_ir_send_from_matrix(const char *dev_id, int index);

/**
 * @brief Check if a Matrix file exists for a device
 * 
 * @param dev_id Device identifier
 * @return true if exists
 */
bool mgr_ir_matrix_exists(const char *dev_id);
