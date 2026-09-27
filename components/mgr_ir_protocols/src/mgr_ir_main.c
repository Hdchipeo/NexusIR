/**
 * @file mgr_ir_main.c
 * @brief High-level IR Application Layer & Protocol Manager (Phase 3 - Migration & Export/Import)
 */

#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "drv_led.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "mgr_ir_protocols.h"
#include "mgr_ir_waveform.h"
#include "sdkconfig.h"
#include "svc_nvs.h"
#include "drv_ir_rmt.h"
#include "ir_engine.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <esp_rom_sys.h>
#include <esp_rom_crc.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

#define TAG "mgr_ir"

#ifndef CONFIG_APP_IR_RX_GPIO
#define IR_RX_GPIO -1
#else
#define IR_RX_GPIO CONFIG_APP_IR_RX_GPIO
#endif

#ifndef CONFIG_APP_IR_TX_GPIO
#define IR_TX_GPIO -1
#else
#define IR_TX_GPIO CONFIG_APP_IR_TX_GPIO
#endif

#define RMT_RESOLUTION_HZ 1000000 // 1MHz, 1 tick = 1us
#define APP_IR_MIN_SYMBOLS 20     // Minimum symbols to be considered valid IR
#define MAX_IR_SYMBOLS 600        // Maximum RMT symbol words (~1200 edges)
#define IR_QUEUE_DEPTH 10
#define DEFAULT_LEARN_TIMEOUT_MS 15000

typedef enum {
  IR_EVT_RX_DONE,
  IR_EVT_TIMEOUT
} ir_event_type_t;

typedef struct {
  ir_event_type_t type;
  uint32_t session_id;
  uint32_t num_symbols;
} ir_event_t;

static esp_timer_handle_t s_timeout_timer = NULL;
static QueueHandle_t s_ir_evt_queue = NULL;
static TaskHandle_t s_ir_task_handle = NULL;
static SemaphoreHandle_t s_ir_mutex = NULL;

// Session and state management
static uint32_t s_current_session_id = 0;
static ir_session_state_t s_session_state = IR_STATE_IDLE;
static bool s_is_slave_mode = false;
static mgr_ir_rx_cb_t s_rx_callback = NULL;

// DMA Learning Buffer
static rmt_symbol_word_t *s_learning_symbols = NULL;
static uint32_t s_learning_num_symbols = 0;
static bool s_learning_is_truncated = false;

static void *mgr_ir_malloc(size_t size) {
  void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ptr == NULL) {
    ptr = malloc(size);
  }
  return ptr;
}

static void normalize_key(char *dst, const char *src, size_t max_len) {
  size_t i = 0;
  for (; i < max_len - 1 && src[i]; i++) {
    dst[i] = toupper((unsigned char)src[i]);
  }
  dst[i] = '\0';
}

void mgr_ir_set_rx_callback(mgr_ir_rx_cb_t cb) {
  s_rx_callback = cb;
}

// Ultra-thin, 100% ISR-safe RMT RX Done Callback (HAL-driven)
static bool app_ir_rx_done_callback(size_t num_symbols, void *user_ctx) {
  BaseType_t high_task_wakeup = pdFALSE;
  if (s_ir_evt_queue) {
    ir_event_t evt = {
      .type = IR_EVT_RX_DONE,
      .session_id = s_current_session_id,
      .num_symbols = (uint32_t)num_symbols,
    };
    xQueueSendFromISR(s_ir_evt_queue, &evt, &high_task_wakeup);
  }
  return (high_task_wakeup == pdTRUE);
}

// Timeout timer callback — dispatches timeout event to queue
static void session_timeout_cb(void *arg) {
  if (s_ir_evt_queue) {
    ir_event_t evt = {
      .type = IR_EVT_TIMEOUT,
      .session_id = s_current_session_id,
      .num_symbols = 0,
    };
    xQueueSend(s_ir_evt_queue, &evt, 0);
  }
}

// Background Worker Task: handles migration on boot, all state transitions, and callbacks
static void mgr_ir_worker_task(void *pvParameters) {
  ESP_LOGI(TAG, "IR Worker Task started");

  // Run startup migration check in background without blocking boot
  size_t mig = 0, skp = 0, errs = 0;
  mgr_ir_migrate_all(&mig, &skp, &errs);

  ir_event_t evt;
  while (1) {
    if (xQueueReceive(s_ir_evt_queue, &evt, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    // Drop stale events from outdated sessions (except slave mode)
    if (evt.session_id != s_current_session_id && !s_is_slave_mode) {
      ESP_LOGD(TAG, "Ignoring stale event (evt_sid=%" PRIu32 ", cur_sid=%" PRIu32 ")",
               evt.session_id, s_current_session_id);
      continue;
    }

    switch (evt.type) {
      case IR_EVT_RX_DONE: {
        if (s_is_slave_mode && s_rx_callback) {
          // Slave background listening
          if (evt.num_symbols >= APP_IR_MIN_SYMBOLS) {
            uint32_t logical_count = evt.num_symbols * 2;
            uint16_t *durations = (uint16_t *)malloc(logical_count * sizeof(uint16_t));
            if (durations) {
              const uint16_t *raw_symbols = (const uint16_t *)s_learning_symbols;
              for (uint32_t i = 0; i < logical_count; i++) {
                durations[i] = raw_symbols[i] & 0x7FFF;
              }
              s_rx_callback(durations, logical_count);
              free(durations);
            }
          }
          // Re-arm RX for next frame via HAL
          ir_engine_rx_start(s_learning_symbols,
                             MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t));
          break;
        }

        // Learning Mode processing
        if (s_session_state == IR_STATE_ARMED) {
          if (evt.num_symbols < APP_IR_MIN_SYMBOLS) {
            ESP_LOGW(TAG, "Session %" PRIu32 ": IR Noise (%" PRIu32 " symbols < %d). Re-arming...",
                     evt.session_id, evt.num_symbols, APP_IR_MIN_SYMBOLS);
            drv_led_set_state(DRV_LED_IR_FAIL);

            // Debounce delay to let optical noise settle
            vTaskDelay(pdMS_TO_TICKS(300));
            if (s_session_state == IR_STATE_ARMED && evt.session_id == s_current_session_id) {
              drv_led_set_state(DRV_LED_IR_LEARN);
              ir_engine_rx_start(s_learning_symbols,
                                 MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t));
            }
          } else {
            // Valid signal captured!
            esp_timer_stop(s_timeout_timer);
            ir_engine_rx_stop();

            xSemaphoreTake(s_ir_mutex, portMAX_DELAY);
            s_learning_num_symbols = (evt.num_symbols > MAX_IR_SYMBOLS)
                                         ? MAX_IR_SYMBOLS
                                         : evt.num_symbols;
            s_learning_is_truncated = (evt.num_symbols >= MAX_IR_SYMBOLS);
            s_session_state = IR_STATE_CAPTURED;
            xSemaphoreGive(s_ir_mutex);

            drv_led_set_state(DRV_LED_IR_SUCCESS);
            ESP_LOGI(TAG, "Session %" PRIu32 " CAPTURED: %" PRIu32 " symbols%s",
                     evt.session_id, s_learning_num_symbols,
                     s_learning_is_truncated ? " (TRUNCATED)" : "");

            // Flash SUCCESS for 1 second, then revert to IDLE
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (s_session_state == IR_STATE_CAPTURED) {
              drv_led_set_state(DRV_LED_IDLE);
            }
          }
        }
        break;
      }

      case IR_EVT_TIMEOUT: {
        if (s_session_state == IR_STATE_ARMED) {
          ESP_LOGW(TAG, "Learn Session %" PRIu32 " TIMED OUT after 15s without valid IR signal",
                   evt.session_id);
          ir_engine_rx_stop();

          xSemaphoreTake(s_ir_mutex, portMAX_DELAY);
          s_session_state = IR_STATE_TIMEOUT;
          xSemaphoreGive(s_ir_mutex);

          drv_led_set_state(DRV_LED_IDLE);
        }
        break;
      }

      default:
        break;
    }
  }
}

esp_err_t mgr_ir_init(void) {
  if (IR_RX_GPIO == -1 || IR_TX_GPIO == -1) {
    ESP_LOGI(TAG, "IR GPIOs not configured, skipping IR init");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing IR RX on GPIO %d, TX on GPIO %d", IR_RX_GPIO, IR_TX_GPIO);

  if (s_ir_mutex == NULL) {
    s_ir_mutex = xSemaphoreCreateMutex();
    if (!s_ir_mutex) {
      ESP_LOGE(TAG, "Failed to create IR mutex");
      return ESP_ERR_NO_MEM;
    }
  }

  if (s_ir_evt_queue == NULL) {
    s_ir_evt_queue = xQueueCreate(IR_QUEUE_DEPTH, sizeof(ir_event_t));
    if (!s_ir_evt_queue) {
      ESP_LOGE(TAG, "Failed to create IR event queue");
      return ESP_ERR_NO_MEM;
    }
  }

  s_learning_symbols = (rmt_symbol_word_t *)heap_caps_malloc(
      MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t),
      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!s_learning_symbols) {
    ESP_LOGE(TAG, "Failed to allocate learning DMA buffer");
    return ESP_ERR_NO_MEM;
  }

  ir_rx_engine_config_t rx_cfg = {
      .gpio_num = IR_RX_GPIO,
      .resolution_hz = RMT_RESOLUTION_HZ,
      .mem_block_symbols = 64,
      .rx_done_cb = app_ir_rx_done_callback,
      .user_ctx = NULL,
  };
  esp_err_t rx_init_err = ir_engine_rx_init(&rx_cfg);
  if (rx_init_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize IR RX Engine: %s", esp_err_to_name(rx_init_err));
    return rx_init_err;
  }

  // Initialize TX Engine
  ir_engine_config_t engine_cfg = {
      .gpio_num = IR_TX_GPIO,
      .resolution_hz = RMT_RESOLUTION_HZ,
  };
  ESP_ERROR_CHECK(ir_engine_init(&engine_cfg));

  // Session Timeout Timer (15s)
  esp_timer_create_args_t timer_args = {
      .callback = session_timeout_cb,
      .name = "ir_timeout",
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_timeout_timer));

  // Launch Worker Task
  if (s_ir_task_handle == NULL) {
    BaseType_t ret = xTaskCreate(mgr_ir_worker_task, "ir_worker", 4096, NULL, 5, &s_ir_task_handle);
    if (ret != pdPASS) {
      ESP_LOGE(TAG, "Failed to spawn IR worker task");
      return ESP_FAIL;
    }
  }

  ESP_LOGI(TAG, "IR Core V3 (Event-Driven + Migration Engine) Initialized");
  return ESP_OK;
}

esp_err_t mgr_ir_start_slave(void) {
  ESP_LOGI(TAG, "Starting IR Slave Mode...");
  s_is_slave_mode = true;
  return ir_engine_rx_start(s_learning_symbols,
                            MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t));
}

esp_err_t mgr_ir_start_learn_with_timeout(uint32_t timeout_ms, uint32_t *out_session_id) {
  if (!s_ir_mutex)
    return ESP_ERR_INVALID_STATE;

  xSemaphoreTake(s_ir_mutex, portMAX_DELAY);
  s_current_session_id++;
  uint32_t session_id = s_current_session_id;
  s_session_state = IR_STATE_ARMED;
  s_learning_num_symbols = 0;
  s_learning_is_truncated = false;
  xSemaphoreGive(s_ir_mutex);

  esp_timer_stop(s_timeout_timer);

  ir_engine_rx_stop();

  esp_err_t err = ir_engine_rx_start(s_learning_symbols,
                                    MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ir_engine_rx_start failed: %s", esp_err_to_name(err));
    xSemaphoreTake(s_ir_mutex, portMAX_DELAY);
    s_session_state = IR_STATE_ERROR;
    xSemaphoreGive(s_ir_mutex);
    return err;
  }

  drv_led_set_state(DRV_LED_IR_LEARN);

  uint64_t timeout_us = (uint64_t)timeout_ms * 1000ULL;
  esp_timer_start_once(s_timeout_timer, timeout_us);

  ESP_LOGI(TAG, "Started Learn Session %" PRIu32 " (Timeout: %" PRIu32 " ms)", session_id, timeout_ms);
  if (out_session_id) *out_session_id = session_id;
  return ESP_OK;
}

esp_err_t mgr_ir_start_learn(void) {
  return mgr_ir_start_learn_with_timeout(DEFAULT_LEARN_TIMEOUT_MS, NULL);
}

esp_err_t mgr_ir_stop_learn(void) {
  if (!s_ir_mutex)
    return ESP_ERR_INVALID_STATE;

  xSemaphoreTake(s_ir_mutex, portMAX_DELAY);
  s_current_session_id++; // Invalidate any pending events for the current session
  s_session_state = IR_STATE_CANCELLED;
  xSemaphoreGive(s_ir_mutex);

  esp_timer_stop(s_timeout_timer);
  ir_engine_rx_stop();
  drv_led_set_state(DRV_LED_IDLE);

  ESP_LOGI(TAG, "Learn session cancelled by user");
  return ESP_OK;
}

esp_err_t mgr_ir_cancel_learn(void) {
  return mgr_ir_stop_learn();
}

bool mgr_ir_is_data_ready(void) {
  return (s_session_state == IR_STATE_CAPTURED && s_learning_num_symbols >= APP_IR_MIN_SYMBOLS);
}

bool mgr_ir_get_learn_status(uint32_t *count) {
  if (count)
    *count = s_learning_num_symbols;
  return (s_session_state == IR_STATE_ARMED);
}

esp_err_t mgr_ir_get_session_info(mgr_ir_session_info_t *out_info) {
  if (!out_info || !s_ir_mutex)
    return ESP_ERR_INVALID_ARG;

  if (xSemaphoreTake(s_ir_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  out_info->session_id = s_current_session_id;
  out_info->state = s_session_state;
  out_info->symbol_count = s_learning_num_symbols;
  out_info->is_truncated = s_learning_is_truncated;
  xSemaphoreGive(s_ir_mutex);
  return ESP_OK;
}

esp_err_t mgr_ir_copy_learning_buffer(rmt_symbol_word_t *dst, size_t max_words,
                                      size_t *out_words, bool *out_is_truncated) {
  if (!dst || max_words == 0 || !s_learning_symbols) return ESP_ERR_INVALID_ARG;
  if (!s_ir_mutex) return ESP_ERR_INVALID_STATE;

  if (xSemaphoreTake(s_ir_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (s_session_state != IR_STATE_CAPTURED && s_learning_num_symbols < APP_IR_MIN_SYMBOLS) {
    xSemaphoreGive(s_ir_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  size_t copy_words = (s_learning_num_symbols > max_words) ? max_words : s_learning_num_symbols;
  memcpy(dst, s_learning_symbols, copy_words * sizeof(rmt_symbol_word_t));

  if (out_words) *out_words = copy_words;
  if (out_is_truncated) *out_is_truncated = s_learning_is_truncated;

  xSemaphoreGive(s_ir_mutex);
  return ESP_OK;
}

esp_err_t mgr_ir_save_learned_with_params(const char *key, uint8_t repeat_count, uint16_t repeat_gap_ms) {
  char upper_key[32] = {0};
  normalize_key(upper_key, key, sizeof(upper_key));

  rmt_symbol_word_t *raw_buf = (rmt_symbol_word_t *)mgr_ir_malloc(MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t));
  if (!raw_buf) return ESP_ERR_NO_MEM;

  size_t captured_words = 0;
  bool is_truncated = false;
  esp_err_t err = mgr_ir_copy_learning_buffer(raw_buf, MAX_IR_SYMBOLS, &captured_words, &is_truncated);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Insufficient IR data symbols to save for %s", upper_key);
    free(raw_buf);
    return err;
  }

  rmt_symbol_word_t *clean_buf = (rmt_symbol_word_t *)mgr_ir_malloc(MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t));
  if (!clean_buf) {
    free(raw_buf);
    return ESP_ERR_NO_MEM;
  }

  size_t clean_words = 0;
  size_t clean_durations = 0;
  err = ir_waveform_normalize(raw_buf, captured_words, clean_buf, MAX_IR_SYMBOLS,
                              &clean_words, &clean_durations);
  free(raw_buf);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Waveform normalization failed for %s: %s", upper_key, esp_err_to_name(err));
    free(clean_buf);
    return err;
  }

  uint8_t flags = is_truncated ? IR_FLAG_TRUNCATED : IR_FLAG_NONE;
  uint8_t *v1_record = NULL;
  size_t record_size = 0;

  err = ir_waveform_serialize_v1(clean_buf, clean_durations, repeat_count, repeat_gap_ms,
                                 flags, &v1_record, &record_size);
  free(clean_buf);

  if (err != ESP_OK) {
    return err;
  }

  ESP_LOGI(TAG, "Saving V1 IR [%s]: %d durations, %d bytes (Repeats: %d, Truncated: %s)",
           upper_key, (int)clean_durations, (int)record_size, repeat_count, is_truncated ? "YES" : "NO");

  err = svc_nvs_save_ir(upper_key, v1_record, record_size);
  free(v1_record);

  if (err == ESP_OK) {
    xSemaphoreTake(s_ir_mutex, portMAX_DELAY);
    s_session_state = IR_STATE_IDLE; // Reset to IDLE upon successful save
    xSemaphoreGive(s_ir_mutex);
  }
  return err;
}

esp_err_t mgr_ir_save_learned_result(const char *key) {
  uint8_t repeat_count = (strncmp(key, "F_", 2) == 0 || strncmp(key, "f_", 2) == 0) ? 1 : 0;
  return mgr_ir_save_learned_with_params(key, repeat_count, IR_DEFAULT_REPEAT_GAP);
}

esp_err_t mgr_ir_send_key(const char *key) {
  if (s_session_state == IR_STATE_ARMED) {
    ESP_LOGW(TAG, "Cannot send IR while learning session is ARMED");
    return ESP_ERR_INVALID_STATE;
  }

  char upper_key[32] = {0};
  normalize_key(upper_key, key, sizeof(upper_key));

  size_t loaded_size = 0;
  if (svc_nvs_load_ir(upper_key, NULL, &loaded_size) != ESP_OK || loaded_size < 5) {
    ESP_LOGW(TAG, "Key %s not found in NVS", upper_key);
    return ESP_ERR_NOT_FOUND;
  }

  uint8_t *buffer = (uint8_t *)mgr_ir_malloc(loaded_size);
  if (!buffer)
    return ESP_ERR_NO_MEM;

  esp_err_t err = svc_nvs_load_ir(upper_key, buffer, &loaded_size);
  if (err != ESP_OK) {
    free(buffer);
    return err;
  }

  // Automatic On-Access Migration: If legacy format detected, upgrade to V1 with CRC32
  bool is_v1 = (loaded_size >= sizeof(ir_record_header_t) &&
                ((ir_record_header_t *)buffer)->magic == IR_RECORD_MAGIC);
  if (!is_v1) {
    uint8_t def_repeat = (strncmp(upper_key, "F_", 2) == 0) ? 1 : 0;
    uint8_t *v1_buf = NULL;
    size_t v1_size = 0;
    if (ir_waveform_upgrade_legacy(buffer, loaded_size, def_repeat, &v1_buf, &v1_size) == ESP_OK) {
      svc_nvs_save_ir(upper_key, v1_buf, v1_size);
      ESP_LOGI(TAG, "On-Access Migration: Key [%s] upgraded to V1 with CRC32", upper_key);
      free(buffer);
      buffer = v1_buf;
      loaded_size = v1_size;
    }
  }

  // Dynamic Carrier configuration (IR-17)
  if (loaded_size >= sizeof(ir_record_header_t)) {
    const ir_record_header_t *hdr = (const ir_record_header_t *)buffer;
    if (hdr->magic == IR_RECORD_MAGIC) {
      uint32_t freq = (hdr->carrier_freq_hz >= 10000 && hdr->carrier_freq_hz <= 60000)
                          ? hdr->carrier_freq_hz
                          : IR_DEFAULT_CARRIER_HZ;
      float duty = (hdr->duty_cycle >= 10 && hdr->duty_cycle <= 90)
                       ? ((float)hdr->duty_cycle / 100.0f)
                       : 0.33f;
      ir_engine_set_carrier(freq, duty);
    }
  }

  rmt_symbol_word_t *tx_symbols = NULL;
  size_t word_count = 0;
  uint8_t repeat_count = 0;
  uint16_t repeat_gap_ms = IR_DEFAULT_REPEAT_GAP;

  err = ir_waveform_deserialize(buffer, loaded_size, &tx_symbols, &word_count,
                                &repeat_count, &repeat_gap_ms);
  free(buffer);

  if (err != ESP_OK || !tx_symbols || word_count == 0) {
    ESP_LOGE(TAG, "Failed to deserialize IR key %s: %s", upper_key, esp_err_to_name(err));
    if (tx_symbols) free(tx_symbols);
    return (err != ESP_OK) ? err : ESP_FAIL;
  }

  ESP_LOGI(TAG, "Transmitting IR [%s]: %d words, repeats: %d (gap: %d ms)",
           upper_key, (int)word_count, (int)repeat_count, (int)repeat_gap_ms);

  drv_led_set_state(DRV_LED_IR_TX);

  err = ir_engine_send_raw(tx_symbols, word_count);

  for (uint8_t rep = 0; rep < repeat_count && err == ESP_OK; rep++) {
    vTaskDelay(pdMS_TO_TICKS(repeat_gap_ms));
    err = ir_engine_send_raw(tx_symbols, word_count);
  }

  drv_led_set_state(DRV_LED_IDLE);
  free(tx_symbols);
  return err;
}

esp_err_t mgr_ir_send_raw(const uint16_t *durations, size_t count) {
  if (count == 0 || durations == NULL)
    return ESP_ERR_INVALID_ARG;

  size_t alloc_size = count * sizeof(uint16_t);
  if (alloc_size % 4 != 0)
    alloc_size += 2;

  rmt_symbol_word_t *tx_symbols = (rmt_symbol_word_t *)mgr_ir_malloc(alloc_size);
  if (!tx_symbols) {
    ESP_LOGE(TAG, "Failed to allocate memory for %d raw symbols", (int)count);
    return ESP_ERR_NO_MEM;
  }

  uint16_t *tx_raw = (uint16_t *)tx_symbols;
  for (size_t i = 0; i < count; i++) {
    uint16_t level = (i % 2 == 0) ? 1 : 0;
    uint16_t duration = durations[i];
    tx_raw[i] = duration | (level << 15);
  }

  ESP_LOGI(TAG, "Sending Raw IR Signal (%d pulses/spaces)...", (int)count);
  drv_led_set_state(DRV_LED_IR_TX);

  size_t word_count = (alloc_size / sizeof(rmt_symbol_word_t));
  esp_err_t err = ir_engine_send_raw(tx_symbols, word_count);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "IR Send Raw Failed: %s", esp_err_to_name(err));
  }

  drv_led_set_state(DRV_LED_IDLE);
  free(tx_symbols);
  return err;
}

esp_err_t mgr_ir_send_cmd(mgr_ir_cmd_t cmd) {
  return mgr_ir_send_key((cmd == APP_IR_CMD_AC_ON) ? "ac_on" : "ac_off");
}

bool mgr_ir_send_key_exists(const char *prefix, const char *brand,
                            const char *suffix) {
  char short_brand[16] = {0};
  int idx = 0;
  for (int i = 0; brand[i] && idx < 15; i++) {
    char c = brand[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
      short_brand[idx++] = c;
    }
  }

  char key[32];
  snprintf(key, sizeof(key), "%s%s_%s", prefix, short_brand, suffix);

  char upper_key[32] = {0};
  normalize_key(upper_key, key, sizeof(upper_key));

  size_t loaded_size = 0;
  if (svc_nvs_load_ir(upper_key, NULL, &loaded_size) == ESP_OK && loaded_size > 0) {
    return true;
  }
  return false;
}

esp_err_t mgr_ir_migrate_all(size_t *out_migrated, size_t *out_skipped, size_t *out_errors) {
  size_t migrated = 0;
  size_t skipped = 0;
  size_t errors = 0;

  nvs_iterator_t it = NULL;
  esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, "ir_data", NVS_TYPE_BLOB, &it);
  if (err != ESP_OK) {
    if (out_migrated) *out_migrated = 0;
    if (out_skipped) *out_skipped = 0;
    if (out_errors) *out_errors = 0;
    return ESP_OK; // No keys in partition yet
  }

  ESP_LOGI(TAG, "Starting NVS IR records migration check...");

  while (err == ESP_OK && it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);

    size_t len = 0;
    if (svc_nvs_load_ir(info.key, NULL, &len) == ESP_OK && len > 0) {
      uint8_t *buf = (uint8_t *)malloc(len);
      if (buf && svc_nvs_load_ir(info.key, buf, &len) == ESP_OK) {
        if (len >= sizeof(ir_record_header_t) &&
            ((ir_record_header_t *)buf)->magic == IR_RECORD_MAGIC) {
          const ir_record_header_t *hdr = (const ir_record_header_t *)buf;
          uint32_t calc_crc = esp_rom_crc32_le(0, buf + sizeof(ir_record_header_t),
                                               hdr->duration_count * sizeof(uint16_t));
          if (calc_crc == hdr->crc32) {
            skipped++;
          } else {
            ESP_LOGE(TAG, "Corrupt key [%s] detected (CRC mismatch)", info.key);
            errors++;
          }
        } else {
          // Legacy record found!
          uint8_t def_repeat = (strncmp(info.key, "F_", 2) == 0 || strncmp(info.key, "f_", 2) == 0) ? 1 : 0;
          uint8_t *v1_buf = NULL;
          size_t v1_len = 0;
          esp_err_t up_err = ir_waveform_upgrade_legacy(buf, len, def_repeat, &v1_buf, &v1_len);
          if (up_err == ESP_OK && v1_buf && v1_len > 0) {
            svc_nvs_save_ir(info.key, v1_buf, v1_len);
            ESP_LOGI(TAG, "Migrated key [%s] -> V1 (%d bytes, CRC: 0x%08" PRIX32 ")",
                     info.key, (int)v1_len, ((ir_record_header_t *)v1_buf)->crc32);
            free(v1_buf);
            migrated++;
          } else {
            ESP_LOGE(TAG, "Failed to migrate legacy key [%s]: %s", info.key, esp_err_to_name(up_err));
            errors++;
          }
        }
        free(buf);
      }
    }
    err = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);

  ESP_LOGI(TAG, "IR Migration Complete: %zu migrated, %zu verified V1, %zu errors",
           migrated, skipped, errors);

  if (out_migrated) *out_migrated = migrated;
  if (out_skipped) *out_skipped = skipped;
  if (out_errors) *out_errors = errors;

  return ESP_OK;
}

esp_err_t mgr_ir_export_key_json(const char *key, char **out_json) {
  if (!key || !out_json) return ESP_ERR_INVALID_ARG;

  char upper_key[32] = {0};
  normalize_key(upper_key, key, sizeof(upper_key));

  size_t loaded_size = 0;
  if (svc_nvs_load_ir(upper_key, NULL, &loaded_size) != ESP_OK || loaded_size < 5) {
    return ESP_ERR_NOT_FOUND;
  }

  uint8_t *buffer = (uint8_t *)mgr_ir_malloc(loaded_size);
  if (!buffer) return ESP_ERR_NO_MEM;
  svc_nvs_load_ir(upper_key, buffer, &loaded_size);

  bool is_v1 = (loaded_size >= sizeof(ir_record_header_t) &&
                ((ir_record_header_t *)buffer)->magic == IR_RECORD_MAGIC);
  if (!is_v1) {
    uint8_t def_rep = (strncmp(upper_key, "F_", 2) == 0) ? 1 : 0;
    uint8_t *v1_buf = NULL;
    size_t v1_size = 0;
    if (ir_waveform_upgrade_legacy(buffer, loaded_size, def_rep, &v1_buf, &v1_size) == ESP_OK) {
      free(buffer);
      buffer = v1_buf;
      loaded_size = v1_size;
    }
  }

  const ir_record_header_t *hdr = (const ir_record_header_t *)buffer;
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "key", upper_key);
  cJSON_AddNumberToObject(root, "version", hdr->version);
  cJSON_AddNumberToObject(root, "carrier_freq_hz", hdr->carrier_freq_hz);
  cJSON_AddNumberToObject(root, "repeat_count", hdr->repeat_count);
  cJSON_AddNumberToObject(root, "repeat_gap_ms", hdr->repeat_gap_ms);
  cJSON_AddBoolToObject(root, "truncated", (hdr->flags & IR_FLAG_TRUNCATED) != 0);

  char crc_str[16];
  snprintf(crc_str, sizeof(crc_str), "0x%08" PRIX32, hdr->crc32);
  cJSON_AddStringToObject(root, "crc32", crc_str);

  cJSON *durations_arr = cJSON_CreateArray();
  const uint16_t *payload = (const uint16_t *)(buffer + sizeof(ir_record_header_t));
  for (uint16_t i = 0; i < hdr->duration_count; i++) {
    cJSON_AddItemToArray(durations_arr, cJSON_CreateNumber(payload[i] & 0x7FFF));
  }
  cJSON_AddItemToObject(root, "durations", durations_arr);

  *out_json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  free(buffer);

  return (*out_json != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t mgr_ir_import_key_json(const char *key, const char *json_str) {
  if (!key || !json_str) return ESP_ERR_INVALID_ARG;

  cJSON *root = cJSON_Parse(json_str);
  if (!root) return ESP_ERR_INVALID_ARG;

  cJSON *dur_arr = cJSON_GetObjectItem(root, "durations");
  if (!dur_arr || !cJSON_IsArray(dur_arr)) {
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  }

  int count = cJSON_GetArraySize(dur_arr);
  if (count < 10 || count > 1200) {
    cJSON_Delete(root);
    return ESP_ERR_INVALID_SIZE;
  }

  uint8_t repeat_count = 0;
  cJSON *rep_item = cJSON_GetObjectItem(root, "repeat_count");
  if (rep_item && cJSON_IsNumber(rep_item)) {
    repeat_count = (uint8_t)rep_item->valueint;
  }

  uint16_t repeat_gap_ms = IR_DEFAULT_REPEAT_GAP;
  cJSON *gap_item = cJSON_GetObjectItem(root, "repeat_gap_ms");
  if (gap_item && cJSON_IsNumber(gap_item)) {
    repeat_gap_ms = (uint16_t)gap_item->valueint;
  }

  size_t words_needed = (count + 1) / 2;
  rmt_symbol_word_t *raw_syms = (rmt_symbol_word_t *)calloc(words_needed + 1, sizeof(rmt_symbol_word_t));
  if (!raw_syms) {
    cJSON_Delete(root);
    return ESP_ERR_NO_MEM;
  }

  uint16_t *raw_u16 = (uint16_t *)raw_syms;
  for (int i = 0; i < count; i++) {
    cJSON *item = cJSON_GetArrayItem(dur_arr, i);
    uint16_t duration = (uint16_t)(item ? item->valueint : 0);
    uint16_t level = (i % 2 == 0) ? 1 : 0;
    raw_u16[i] = duration | (level << 15);
  }
  cJSON_Delete(root);

  rmt_symbol_word_t *clean_syms = (rmt_symbol_word_t *)calloc(words_needed + 1, sizeof(rmt_symbol_word_t));
  if (!clean_syms) {
    free(raw_syms);
    return ESP_ERR_NO_MEM;
  }

  size_t clean_words = 0;
  size_t clean_durations = 0;
  esp_err_t err = ir_waveform_normalize(raw_syms, words_needed, clean_syms, words_needed,
                                        &clean_words, &clean_durations);
  free(raw_syms);

  if (err != ESP_OK) {
    free(clean_syms);
    return err;
  }

  uint8_t *v1_buf = NULL;
  size_t v1_size = 0;
  err = ir_waveform_serialize_v1(clean_syms, clean_durations, repeat_count, repeat_gap_ms,
                                 IR_FLAG_NONE, &v1_buf, &v1_size);
  free(clean_syms);

  if (err != ESP_OK) {
    return err;
  }

  char upper_key[32] = {0};
  normalize_key(upper_key, key, sizeof(upper_key));

  err = svc_nvs_save_ir(upper_key, v1_buf, v1_size);
  free(v1_buf);

  ESP_LOGI(TAG, "Imported IR key [%s]: %d durations (%d bytes)", upper_key, (int)clean_durations, (int)v1_size);
  return err;
}
