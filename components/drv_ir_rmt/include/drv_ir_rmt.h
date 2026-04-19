/**
 * @file drv_ir_rmt.h
 * @brief IR RMT Driver (Hardware Abstraction)
 */

#pragma once

#include "esp_err.h"
#include "ir_types.h" // For ir_engine_config_t if defined there

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the IR RMT engine
 *
 * @param config Configuration struct (GPIO, resolution etc)
 * @return esp_err_t
 */
esp_err_t ir_engine_init(const ir_engine_config_t *config);

/**
 * @brief Send raw RMT symbols
 *
 * @param symbols Pointer to rmt_symbol_word_t array
 * @param count Number of symbols
 * @return esp_err_t
 */
esp_err_t ir_engine_send_raw(const void *symbols, size_t count);

#ifdef __cplusplus
}
#endif
