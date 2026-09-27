#ifndef SYS_MEM_H
#define SYS_MEM_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize memory monitoring task
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sys_mem_init(void);

/**
 * @brief Get free internal heap size
 *
 * @return size_t Free bytes
 */
size_t sys_mem_get_free_internal(void);

/**
 * @brief Get free PSRAM size
 *
 * @return size_t Free bytes
 */
size_t sys_mem_get_free_psram(void);

/**
 * @brief Check if enough memory is available for an operation
 *
 * @param size Required size in bytes
 * @param use_psram Whether to check PSRAM or internal heap
 * @return bool True if enough memory is available
 */
bool sys_mem_is_safe(size_t size, bool use_psram);

#endif // SYS_MEM_H
