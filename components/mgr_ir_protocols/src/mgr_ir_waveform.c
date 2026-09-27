/**
 * @file mgr_ir_waveform.c
 * @brief IR Waveform Processing, Serialization and Normalization Engine
 */

#include "mgr_ir_waveform.h"
#include <string.h>
#include <stdlib.h>

#if defined(ESP_PLATFORM)
#include <esp_log.h>
#include <esp_rom_crc.h>
static const char *TAG = "mgr_ir_waveform";
#else
#include <stdio.h>
static const char *TAG = "mgr_ir_waveform";
#define ESP_LOGI(tag, fmt, ...) printf("[INFO][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[WARN][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("[ERR ][%s] " fmt "\n", tag, ##__VA_ARGS__)

static inline uint32_t esp_rom_crc32_le(uint32_t crc, const uint8_t *buf, size_t len) {
    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

const char *esp_err_to_name(esp_err_t code) {
    switch (code) {
        case ESP_OK: return "ESP_OK";
        case ESP_FAIL: return "ESP_FAIL";
        case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
        case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE";
        case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
        case ESP_ERR_NOT_SUPPORTED: return "ESP_ERR_NOT_SUPPORTED";
        case ESP_ERR_INVALID_RESPONSE: return "ESP_ERR_INVALID_RESPONSE";
        case ESP_ERR_INVALID_CRC: return "ESP_ERR_INVALID_CRC";
        default: return "UNKNOWN";
    }
}
#endif

esp_err_t ir_waveform_normalize_durations(const uint16_t *src_u16, size_t src_duration_count,
                                          rmt_symbol_word_t *dst_symbols, size_t max_dst_words,
                                          size_t *out_word_count, size_t *out_duration_count) {
    if (!src_u16 || src_duration_count == 0 || !dst_symbols || max_dst_words == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t *dst_u16 = (uint16_t *)dst_symbols;
    size_t max_dst_halfwords = max_dst_words * 2;

    // Detect if input array has explicit level bit encoded in bit 15 (e.g. from RMT RX)
    // Pure duration arrays (Legacy Matrix, JSON) have bit 15 = 0.
    bool has_level_encoding = false;
    for (size_t i = 0; i < src_duration_count && i < 16; i++) {
        if ((src_u16[i] >> 15) & 0x1) {
            has_level_encoding = true;
            break;
        }
    }

    size_t start_idx = 0;
    if (has_level_encoding) {
        // Strip leading spaces (level=0) and leading zero-durations
        while (start_idx < src_duration_count) {
            uint16_t val = src_u16[start_idx];
            uint16_t level = (val >> 15) & 0x1;
            uint16_t duration = val & 0x7FFF;

            if (level == 1 && duration > 0) {
                break; // Found first valid Mark
            }
            start_idx++;
        }
    } else {
        // Pure durations: index 0 is implicitly Mark. Skip any initial 0-duration artifact
        while (start_idx < src_duration_count && (src_u16[start_idx] & 0x7FFF) == 0) {
            start_idx++;
        }
    }

    if (start_idx >= src_duration_count) {
        ESP_LOGW(TAG, "Normalize failed: no valid Mark pulse found");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // 2. Copy and format alternating Mark/Space pulses
    size_t out_idx = 0;
    for (size_t i = start_idx; i < src_duration_count; i++) {
        uint16_t val = src_u16[i];
        uint16_t duration = val & 0x7FFF;

        if (duration == 0) {
            continue;
        }

        if (out_idx >= max_dst_halfwords) {
            ESP_LOGW(TAG, "Destination buffer full while normalizing (%d items)", (int)out_idx);
            break;
        }

        uint16_t expected_level = (out_idx % 2 == 0) ? 1 : 0;
        dst_u16[out_idx++] = duration | (expected_level << 15);
    }

    // 3. Trim trailing zero-durations
    while (out_idx > 0 && (dst_u16[out_idx - 1] & 0x7FFF) == 0) {
        out_idx--;
    }

    // 4. Ensure odd count (ends at Mark pulse)
    if (out_idx % 2 == 0 && out_idx > 0) {
        out_idx--;
    }

    if (out_idx < 1) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Zero out unused slot in last symbol word
    if (out_idx % 2 != 0) {
        dst_u16[out_idx] = 0;
    }

    size_t words = (out_idx + 1) / 2;
    if (out_word_count) *out_word_count = words;
    if (out_duration_count) *out_duration_count = out_idx;

    return ESP_OK;
}

esp_err_t ir_waveform_normalize(const rmt_symbol_word_t *src_symbols, size_t src_word_count,
                                rmt_symbol_word_t *dst_symbols, size_t max_dst_words,
                                size_t *out_word_count, size_t *out_duration_count) {
    if (!src_symbols || src_word_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return ir_waveform_normalize_durations((const uint16_t *)src_symbols, src_word_count * 2,
                                           dst_symbols, max_dst_words,
                                           out_word_count, out_duration_count);
}

esp_err_t ir_waveform_serialize_v1(const rmt_symbol_word_t *symbols, size_t duration_count,
                                   uint8_t repeat_count, uint16_t repeat_gap_ms, uint8_t flags,
                                   uint8_t **out_buffer, size_t *out_size) {
    if (!symbols || duration_count == 0 || !out_buffer || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t payload_bytes = duration_count * sizeof(uint16_t);
    size_t total_size = sizeof(ir_record_header_t) + payload_bytes;

    uint8_t *buf = (uint8_t *)malloc(total_size);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for V1 record", (int)total_size);
        return ESP_ERR_NO_MEM;
    }

    ir_record_header_t *hdr = (ir_record_header_t *)buf;
    hdr->magic = IR_RECORD_MAGIC;
    hdr->version = IR_RECORD_VERSION_V1;
    hdr->flags = flags;
    hdr->carrier_freq_hz = IR_DEFAULT_CARRIER_HZ;
    hdr->duty_cycle = IR_DEFAULT_DUTY_CYCLE;
    hdr->repeat_count = repeat_count;
    hdr->repeat_gap_ms = repeat_gap_ms;
    hdr->duration_count = (uint16_t)duration_count;

    hdr->crc32 = esp_rom_crc32_le(0, (const uint8_t *)symbols, payload_bytes);
    memcpy(buf + sizeof(ir_record_header_t), symbols, payload_bytes);

    *out_buffer = buf;
    *out_size = total_size;

    ESP_LOGI(TAG, "Serialized V1 record: %d durations, %d bytes, CRC: 0x%08X",
             (int)duration_count, (int)total_size, (unsigned)hdr->crc32);

    return ESP_OK;
}

esp_err_t ir_waveform_deserialize(const uint8_t *raw_data, size_t raw_len,
                                  rmt_symbol_word_t **out_symbols, size_t *out_word_count,
                                  uint8_t *out_repeat_count, uint16_t *out_repeat_gap_ms) {
    if (!raw_data || raw_len == 0 || !out_symbols || !out_word_count) {
        return ESP_ERR_INVALID_ARG;
    }

    // CASE 1: V1 Format
    if (raw_len >= sizeof(ir_record_header_t)) {
        const ir_record_header_t *hdr = (const ir_record_header_t *)raw_data;
        if (hdr->magic == IR_RECORD_MAGIC) {
            if (hdr->version != IR_RECORD_VERSION_V1) {
                ESP_LOGE(TAG, "Unsupported V1 version: %d", hdr->version);
                return ESP_ERR_NOT_SUPPORTED;
            }

            size_t payload_bytes = hdr->duration_count * sizeof(uint16_t);
            if (raw_len < sizeof(ir_record_header_t) + payload_bytes) {
                ESP_LOGE(TAG, "V1 Record truncated: expected %d bytes, got %d",
                         (int)(sizeof(ir_record_header_t) + payload_bytes), (int)raw_len);
                return ESP_ERR_INVALID_SIZE;
            }

            uint32_t calc_crc = esp_rom_crc32_le(0, raw_data + sizeof(ir_record_header_t), payload_bytes);
            if (calc_crc != hdr->crc32) {
                ESP_LOGE(TAG, "CRC32 mismatch! Header: 0x%08X, Computed: 0x%08X",
                         (unsigned)hdr->crc32, (unsigned)calc_crc);
                return ESP_ERR_INVALID_CRC;
            }

            size_t alloc_words = (hdr->duration_count + 2) / 2 + 1;
            rmt_symbol_word_t *dst_syms = (rmt_symbol_word_t *)calloc(1, alloc_words * sizeof(rmt_symbol_word_t));
            if (!dst_syms) return ESP_ERR_NO_MEM;

            const uint16_t *src_u16 = (const uint16_t *)(raw_data + sizeof(ir_record_header_t));
            size_t clean_words = 0;
            size_t clean_durations = 0;
            esp_err_t err = ir_waveform_normalize_durations(src_u16, hdr->duration_count, dst_syms, alloc_words,
                                                            &clean_words, &clean_durations);
            if (err != ESP_OK) {
                free(dst_syms);
                return err;
            }

            *out_symbols = dst_syms;
            *out_word_count = clean_words;
            if (out_repeat_count) *out_repeat_count = hdr->repeat_count;
            if (out_repeat_gap_ms) *out_repeat_gap_ms = hdr->repeat_gap_ms;

            return ESP_OK;
        }
    }

    // CASE 2: Legacy RAW Format (0xAA or 0x55 magic at byte 0)
    // Structure: [Magic: 1B][Count: 4B or 2B][Data: count * 2B]
    if (raw_len >= 5 && (raw_data[0] == 0xAA || raw_data[0] == 0x55)) {
        size_t count = 0;
        const uint16_t *src_u16 = NULL;

        // Try 4-byte count header (standard NexusIR format)
        uint32_t count32 = 0;
        memcpy(&count32, raw_data + 1, 4);
        if (count32 > 0 && raw_len >= 5 + count32 * sizeof(uint16_t)) {
            count = count32;
            src_u16 = (const uint16_t *)(raw_data + 5);
        } else {
            // Fallback: 2-byte count header
            uint16_t count16 = (uint16_t)raw_data[1] | ((uint16_t)raw_data[2] << 8);
            if (count16 > 0 && raw_len >= 3 + count16 * sizeof(uint16_t)) {
                count = count16;
                src_u16 = (const uint16_t *)(raw_data + 3);
            }
        }

        if (!src_u16 || count == 0) {
            ESP_LOGE(TAG, "Legacy RAW invalid header or truncated (len=%d)", (int)raw_len);
            return ESP_ERR_INVALID_SIZE;
        }

        size_t alloc_words = (count + 2) / 2 + 1;
        rmt_symbol_word_t *dst_syms = (rmt_symbol_word_t *)calloc(1, alloc_words * sizeof(rmt_symbol_word_t));
        if (!dst_syms) return ESP_ERR_NO_MEM;

        size_t clean_words = 0;
        size_t clean_durations = 0;
        esp_err_t err = ir_waveform_normalize_durations(src_u16, count, dst_syms, alloc_words,
                                                        &clean_words, &clean_durations);
        if (err != ESP_OK) {
            free(dst_syms);
            return err;
        }

        *out_symbols = dst_syms;
        *out_word_count = clean_words;
        if (out_repeat_count) *out_repeat_count = 0;
        if (out_repeat_gap_ms) *out_repeat_gap_ms = IR_DEFAULT_REPEAT_GAP;

        ESP_LOGI(TAG, "Loaded & normalized Legacy RAW: %d symbols -> %d clean words",
                 (int)count, (int)clean_words);
        return ESP_OK;
    }

    // CASE 3: Legacy Matrix Format (Raw array of uint16_t, no magic)
    if (raw_len >= 20 && (raw_len % sizeof(uint16_t) == 0)) {
        size_t count = raw_len / sizeof(uint16_t);
        const uint16_t *src_u16 = (const uint16_t *)raw_data;

        size_t alloc_words = (count + 2) / 2 + 1;
        rmt_symbol_word_t *dst_syms = (rmt_symbol_word_t *)calloc(1, alloc_words * sizeof(rmt_symbol_word_t));
        if (!dst_syms) return ESP_ERR_NO_MEM;

        size_t clean_words = 0;
        size_t clean_durations = 0;
        esp_err_t err = ir_waveform_normalize_durations(src_u16, count, dst_syms, alloc_words,
                                                        &clean_words, &clean_durations);
        if (err != ESP_OK) {
            free(dst_syms);
            return err;
        }

        *out_symbols = dst_syms;
        *out_word_count = clean_words;
        if (out_repeat_count) *out_repeat_count = 0;
        if (out_repeat_gap_ms) *out_repeat_gap_ms = IR_DEFAULT_REPEAT_GAP;

        ESP_LOGI(TAG, "Loaded & normalized Legacy Matrix: %d durations -> %d clean words",
                 (int)count, (int)clean_words);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Unrecognized IR record format (len=%d, magic=0x%02X)", (int)raw_len, raw_data[0]);
    return ESP_ERR_INVALID_RESPONSE;
}

esp_err_t ir_waveform_upgrade_legacy(const uint8_t *old_data, size_t old_len,
                                     uint8_t default_repeat_count,
                                     uint8_t **out_v1_data, size_t *out_v1_len) {
    if (!old_data || old_len == 0 || !out_v1_data || !out_v1_len) {
        return ESP_ERR_INVALID_ARG;
    }

    if (old_len >= sizeof(ir_record_header_t)) {
        const ir_record_header_t *hdr = (const ir_record_header_t *)old_data;
        if (hdr->magic == IR_RECORD_MAGIC) {
            return ESP_ERR_INVALID_STATE; // Already V1, no upgrade needed
        }
    }

    rmt_symbol_word_t *syms = NULL;
    size_t word_count = 0;
    uint8_t repeat_count = 0;
    uint16_t repeat_gap_ms = IR_DEFAULT_REPEAT_GAP;

    esp_err_t err = ir_waveform_deserialize(old_data, old_len, &syms, &word_count,
                                            &repeat_count, &repeat_gap_ms);
    if (err != ESP_OK || !syms || word_count == 0) {
        if (syms) free(syms);
        return err;
    }

    // Apply caller's requested default repeat policy (e.g. 1 for Fans, 0 for AC)
    repeat_count = default_repeat_count;

    size_t duration_count = word_count * 2;
    const uint16_t *u16 = (const uint16_t *)syms;
    if ((u16[duration_count - 1] & 0x7FFF) == 0) {
        duration_count--;
    }

    err = ir_waveform_serialize_v1(syms, duration_count, repeat_count, repeat_gap_ms,
                                  IR_FLAG_NONE, out_v1_data, out_v1_len);
    free(syms);

    return err;
}
