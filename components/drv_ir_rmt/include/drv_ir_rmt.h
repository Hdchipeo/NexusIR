/**
 * @file drv_ir_rmt.h
 * @brief IR RMT Driver (Hardware Abstraction Layer for TX & RX)
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ir_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Prototype for low-overhead RX Done ISR callback
 * @param num_symbols Number of RMT symbols received
 * @param user_ctx User context pointer passed during rx_init
 * @return true if a higher-priority task was woken up (for FreeRTOS portYIELD_FROM_ISR)
 */
typedef bool (*ir_rx_done_isr_callback_t)(size_t num_symbols, void *user_ctx);

/**
 * @brief Configuration structure for IR RMT RX Channel
 */
typedef struct {
    int gpio_num;                           /**< GPIO connected to IR Demodulator (e.g. GPIO 7) */
    uint32_t resolution_hz;                 /**< Channel resolution (e.g. 1000000 for 1us ticks) */
    size_t mem_block_symbols;               /**< Memory block symbols (0 for default 64) */
    ir_rx_done_isr_callback_t rx_done_cb;   /**< Callback invoked directly from RMT ISR */
    void *user_ctx;                         /**< Context pointer passed to callback */
} ir_rx_engine_config_t;

/**
 * @brief Initialize the IR RMT TX engine
 *
 * @param config Configuration struct (GPIO, resolution etc)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_init(const ir_engine_config_t *config);

/**
 * @brief Dynamically configure or update the IR carrier frequency and duty cycle
 *
 * @param freq_hz Carrier frequency in Hz (e.g. 36000, 38000, 40000, 56000)
 * @param duty_cycle Carrier duty cycle in float (e.g. 0.33 for 33%)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_set_carrier(uint32_t freq_hz, float duty_cycle);

/**
 * @brief Send raw RMT symbols (blocks until transmission finishes or timeout)
 *
 * @param symbols Pointer to rmt_symbol_word_t array
 * @param count Number of symbols
 * @return esp_err_t ESP_OK on success, ESP_ERR_TIMEOUT, or other error
 */
esp_err_t ir_engine_send_raw(const void *symbols, size_t count);

/**
 * @brief Initialize the IR RMT RX engine
 *
 * @param config Configuration struct
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_rx_init(const ir_rx_engine_config_t *config);

/**
 * @brief Arm RMT RX to receive incoming IR pulse symbols into the designated buffer
 *
 * @param buffer Pointer to DMA/internal RAM symbol buffer
 * @param buffer_size_bytes Size of the buffer in bytes
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_rx_start(void *buffer, size_t buffer_size_bytes);

/**
 * @brief Stop / Abort RMT RX reception
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_rx_stop(void);

#ifdef __cplusplus
}
#endif
