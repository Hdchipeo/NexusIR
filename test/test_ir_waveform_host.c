/**
 * @file test_ir_waveform_host.c
 * @brief Host Unit Tests & Regression Test Suite for IR Waveform Engine (IR-18)
 * Compiles and runs directly on macOS / Linux using clang / gcc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "../components/mgr_ir_protocols/include/mgr_ir_waveform.h"

// Bring in waveform implementation for direct unit-test compilation
#include "../components/mgr_ir_protocols/src/mgr_ir_waveform.c"

#define TEST_PASS() printf("  [PASS] %s\n", __func__)

// 1. Test waveform normalization (strip leading space, filter 0-duration, trim trailing space)
static void test_waveform_normalization(void) {
    // Waveform starting with a leading space (level=0, duration=8900)
    // followed by valid mark (level=1, 9000), space (0, 4500), 0-duration artifact, mark (1, 560), space (0, 560)
    uint16_t raw_input[] = {
        (uint16_t)(8900 | (0 << 15)), // Leading space (to be dropped)
        (uint16_t)(0    | (0 << 15)), // Zero duration (to be dropped)
        (uint16_t)(9000 | (1 << 15)), // Mark 1
        (uint16_t)(4500 | (0 << 15)), // Space 1
        (uint16_t)(0    | (1 << 15)), // Zero duration (to be dropped)
        (uint16_t)(560  | (1 << 15)), // Mark 2
        (uint16_t)(1690 | (0 << 15)), // Space 2
        (uint16_t)(560  | (1 << 15)), // Mark 3 (Ends at Mark)
        (uint16_t)(0    | (0 << 15)), // Trailing zero
    };

    rmt_symbol_word_t clean_syms[16] = {0};
    size_t out_words = 0;
    size_t out_durations = 0;

    esp_err_t err = ir_waveform_normalize_durations(raw_input, sizeof(raw_input) / sizeof(raw_input[0]),
                                                    clean_syms, 16, &out_words, &out_durations);
    assert(err == ESP_OK);
    // After stripping leading space and zero durations, expected durations:
    // Mark(9000), Space(4500), Mark(560), Space(1690), Mark(560) = 5 durations
    assert(out_durations == 5);
    assert(out_words == 3); // (5 + 1) / 2 = 3 words

    uint16_t *u16 = (uint16_t *)clean_syms;
    assert((u16[0] & 0x7FFF) == 9000 && (u16[0] >> 15) == 1);
    assert((u16[1] & 0x7FFF) == 4500 && (u16[1] >> 15) == 0);
    assert((u16[2] & 0x7FFF) == 560  && (u16[2] >> 15) == 1);
    assert((u16[3] & 0x7FFF) == 1690 && (u16[3] >> 15) == 0);
    assert((u16[4] & 0x7FFF) == 560  && (u16[4] >> 15) == 1);

    TEST_PASS();
}

// 2. Test odd duration count buffer over-read bug prevention
static void test_odd_duration_overread_prevention(void) {
    // Array of exactly 11 durations (odd number)
    uint16_t raw_odd[11] = {
        (uint16_t)(9000 | (1 << 15)),
        (uint16_t)(4500 | (0 << 15)),
        (uint16_t)(560  | (1 << 15)),
        (uint16_t)(560  | (0 << 15)),
        (uint16_t)(560  | (1 << 15)),
        (uint16_t)(1690 | (0 << 15)),
        (uint16_t)(560  | (1 << 15)),
        (uint16_t)(560  | (0 << 15)),
        (uint16_t)(560  | (1 << 15)),
        (uint16_t)(1690 | (0 << 15)),
        (uint16_t)(560  | (1 << 15)), // 11th item: Mark
    };

    rmt_symbol_word_t clean_syms[8] = {0};
    size_t out_words = 0;
    size_t out_durations = 0;

    esp_err_t err = ir_waveform_normalize_durations(raw_odd, 11, clean_syms, 8, &out_words, &out_durations);
    assert(err == ESP_OK);
    assert(out_durations == 11);
    assert(out_words == 6); // 11 items packed into 6 words (last halfword is 0)

    uint16_t *u16 = (uint16_t *)clean_syms;
    assert((u16[10] & 0x7FFF) == 560);
    assert((u16[11] & 0x7FFF) == 0); // Padded with 0, no garbage read

    TEST_PASS();
}

// 3. Test V1 serialization, header validation and deserialization
static void test_v1_serialization_and_deserialization(void) {
    uint16_t test_durations[5] = {
        (uint16_t)(9000 | (1 << 15)),
        (uint16_t)(4500 | (0 << 15)),
        (uint16_t)(560  | (1 << 15)),
        (uint16_t)(560  | (0 << 15)),
        (uint16_t)(560  | (1 << 15)),
    };

    rmt_symbol_word_t clean_syms[4] = {0};
    size_t out_words = 0;
    size_t out_durations = 0;
    esp_err_t err = ir_waveform_normalize_durations(test_durations, 5, clean_syms, 4,
                                                    &out_words, &out_durations);
    assert(err == ESP_OK);

    uint8_t *v1_buf = NULL;
    size_t v1_size = 0;
    err = ir_waveform_serialize_v1(clean_syms, out_durations, 1, 45, IR_FLAG_NONE,
                                   &v1_buf, &v1_size);
    assert(err == ESP_OK);
    assert(v1_buf != NULL);
    assert(v1_size == sizeof(ir_record_header_t) + out_durations * sizeof(uint16_t));

    const ir_record_header_t *hdr = (const ir_record_header_t *)v1_buf;
    assert(hdr->magic == IR_RECORD_MAGIC);
    assert(hdr->version == IR_RECORD_VERSION_V1);
    assert(hdr->repeat_count == 1);
    assert(hdr->repeat_gap_ms == 45);
    assert(hdr->duration_count == 5);
    assert(hdr->crc32 != 0);

    // Deserialize
    rmt_symbol_word_t *deser_syms = NULL;
    size_t deser_words = 0;
    uint8_t rep_cnt = 0;
    uint16_t rep_gap = 0;

    err = ir_waveform_deserialize(v1_buf, v1_size, &deser_syms, &deser_words, &rep_cnt, &rep_gap);
    assert(err == ESP_OK);
    assert(deser_words == out_words);
    assert(rep_cnt == 1);
    assert(rep_gap == 45);

    const uint16_t *orig_u16 = (const uint16_t *)clean_syms;
    const uint16_t *deser_u16 = (const uint16_t *)deser_syms;
    for (size_t i = 0; i < out_durations; i++) {
        assert(orig_u16[i] == deser_u16[i]);
    }

    free(v1_buf);
    free(deser_syms);

    TEST_PASS();
}

// 4. Test CRC32 corruption detection
static void test_crc32_corruption_detection(void) {
    uint16_t test_durations[3] = {
        (uint16_t)(9000 | (1 << 15)),
        (uint16_t)(4500 | (0 << 15)),
        (uint16_t)(560  | (1 << 15)),
    };
    rmt_symbol_word_t syms[2] = {0};
    size_t words = 0, durs = 0;
    ir_waveform_normalize_durations(test_durations, 3, syms, 2, &words, &durs);

    uint8_t *v1_buf = NULL;
    size_t v1_size = 0;
    ir_waveform_serialize_v1(syms, durs, 0, 40, IR_FLAG_NONE, &v1_buf, &v1_size);

    // Tamper 1 bit in payload
    v1_buf[sizeof(ir_record_header_t) + 1] ^= 0x01;

    rmt_symbol_word_t *out_syms = NULL;
    size_t out_words = 0;
    esp_err_t err = ir_waveform_deserialize(v1_buf, v1_size, &out_syms, &out_words, NULL, NULL);

    assert(err == ESP_ERR_INVALID_CRC);
    assert(out_syms == NULL);

    free(v1_buf);
    TEST_PASS();
}

// 5. Test Legacy 0xAA RAW format automatic upgrade
static void test_legacy_raw_upgrade(void) {
    uint16_t durations[3] = {9000, 4500, 560};
    uint8_t legacy_raw[3 + 3 * 2];
    legacy_raw[0] = 0xAA; // Magic
    legacy_raw[1] = 3;    // Count low
    legacy_raw[2] = 0;    // Count high
    memcpy(&legacy_raw[3], durations, 3 * sizeof(uint16_t));

    uint8_t *v1_buf = NULL;
    size_t v1_size = 0;
    esp_err_t err = ir_waveform_upgrade_legacy(legacy_raw, sizeof(legacy_raw), 1, &v1_buf, &v1_size);
    assert(err == ESP_OK);
    assert(v1_buf != NULL);

    const ir_record_header_t *hdr = (const ir_record_header_t *)v1_buf;
    assert(hdr->magic == IR_RECORD_MAGIC);
    assert(hdr->version == IR_RECORD_VERSION_V1);
    assert(hdr->repeat_count == 1);
    assert(hdr->duration_count == 3);

    // Deserialize upgraded buffer
    rmt_symbol_word_t *syms = NULL;
    size_t words = 0;
    err = ir_waveform_deserialize(v1_buf, v1_size, &syms, &words, NULL, NULL);
    assert(err == ESP_OK);
    assert(words == 2);

    free(v1_buf);
    free(syms);
    TEST_PASS();
}

// 6. Test Legacy Matrix array automatic upgrade
static void test_legacy_matrix_upgrade(void) {
    // Array of 10 durations (20 bytes, no magic)
    uint16_t matrix_raw[10] = {
        9000, 4500, 560, 560, 560, 1690, 560, 560, 560, 1690
    };

    uint8_t *v1_buf = NULL;
    size_t v1_size = 0;
    esp_err_t err = ir_waveform_upgrade_legacy((const uint8_t *)matrix_raw, sizeof(matrix_raw), 0,
                                               &v1_buf, &v1_size);
    assert(err == ESP_OK);
    assert(v1_buf != NULL);

    const ir_record_header_t *hdr = (const ir_record_header_t *)v1_buf;
    assert(hdr->magic == IR_RECORD_MAGIC);
    assert(hdr->version == IR_RECORD_VERSION_V1);
    assert(hdr->repeat_count == 0); // Matrix is AC, no repeat
    // Since input was 10 items, ending at Space, normalize cuts the last space to end at Mark -> 9 items
    assert(hdr->duration_count == 9);

    free(v1_buf);
    TEST_PASS();
}

int main(void) {
    printf("=====================================================\n");
    printf("  Running NexusIR Waveform Engine Host Test Suite    \n");
    printf("=====================================================\n");

    test_waveform_normalization();
    test_odd_duration_overread_prevention();
    test_v1_serialization_and_deserialization();
    test_crc32_corruption_detection();
    test_legacy_raw_upgrade();
    test_legacy_matrix_upgrade();

    printf("=====================================================\n");
    printf("  ALL 6 HOST UNIT TESTS PASSED SUCCESSFULLY!         \n");
    printf("=====================================================\n");
    return 0;
}
