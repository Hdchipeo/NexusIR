/**
 * @file mgr_ir_waveform.h
 * @brief IR Waveform Processing, Serialization and Normalization Engine
 * Portable across ESP-IDF target (ESP32-C3) and Host Test Environment (x86_64/arm64)
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(ESP_PLATFORM)
#include <esp_err.h>
#include "driver/rmt_types.h"
#else
// Host Unit-Test Environment Shim (POSIX / macOS / Linux)
typedef int esp_err_t;
#define ESP_OK                   0
#define ESP_FAIL                -1
#define ESP_ERR_NO_MEM           0x101
#define ESP_ERR_INVALID_ARG      0x102
#define ESP_ERR_INVALID_STATE    0x103
#define ESP_ERR_INVALID_SIZE     0x104
#define ESP_ERR_NOT_FOUND        0x105
#define ESP_ERR_NOT_SUPPORTED    0x106
#define ESP_ERR_TIMEOUT          0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC      0x109

typedef union {
    struct {
        uint16_t duration0 : 15;
        uint16_t level0 : 1;
        uint16_t duration1 : 15;
        uint16_t level1 : 1;
    };
    uint32_t val;
} rmt_symbol_word_t;

const char *esp_err_to_name(esp_err_t code);
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define IR_RECORD_MAGIC         0x5249584E  // "NXIR" (NexusIR)
#define IR_RECORD_VERSION_V1    1

#define IR_FLAG_NONE            0x00
#define IR_FLAG_TRUNCATED       0x01        // Signal hit buffer limits, possibly truncated
#define IR_FLAG_CARRIER_CUSTOM  0x02        // Non-default carrier
#define IR_FLAG_FAN_REPEAT      0x04        // Device requires repeated transmission

#define IR_DEFAULT_CARRIER_HZ   38000
#define IR_DEFAULT_DUTY_CYCLE   33
#define IR_DEFAULT_REPEAT_GAP   40          // 40ms

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;           /**< Magic identifier: IR_RECORD_MAGIC (0x5249584E) */
    uint8_t  version;         /**< Structure version: 1 */
    uint8_t  flags;           /**< Operational flags (e.g. IR_FLAG_TRUNCATED) */
    uint16_t carrier_freq_hz; /**< Carrier frequency in Hz (e.g. 38000) */
    uint8_t  duty_cycle;      /**< Carrier duty cycle in percent (e.g. 33) */
    uint8_t  repeat_count;    /**< 0 = send once, 1 = send twice (repeat once) */
    uint16_t repeat_gap_ms;   /**< Interval between repeats in ms */
    uint16_t duration_count;  /**< Number of uint16_t elements in payload */
    uint32_t crc32;           /**< CRC32 of payload data */
} ir_record_header_t;
#pragma pack(pop)

esp_err_t ir_waveform_normalize_durations(const uint16_t *src_u16, size_t src_duration_count,
                                          rmt_symbol_word_t *dst_symbols, size_t max_dst_words,
                                          size_t *out_word_count, size_t *out_duration_count);

esp_err_t ir_waveform_normalize(const rmt_symbol_word_t *src_symbols, size_t src_word_count,
                                rmt_symbol_word_t *dst_symbols, size_t max_dst_words,
                                size_t *out_word_count, size_t *out_duration_count);

esp_err_t ir_waveform_serialize_v1(const rmt_symbol_word_t *symbols, size_t duration_count,
                                   uint8_t repeat_count, uint16_t repeat_gap_ms, uint8_t flags,
                                   uint8_t **out_buffer, size_t *out_size);

esp_err_t ir_waveform_deserialize(const uint8_t *raw_data, size_t raw_len,
                                  rmt_symbol_word_t **out_symbols, size_t *out_word_count,
                                  uint8_t *out_repeat_count, uint16_t *out_repeat_gap_ms);

esp_err_t ir_waveform_upgrade_legacy(const uint8_t *old_data, size_t old_len,
                                     uint8_t default_repeat_count,
                                     uint8_t **out_v1_data, size_t *out_v1_len);

#ifdef __cplusplus
}
#endif
