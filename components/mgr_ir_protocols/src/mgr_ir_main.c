#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "drv_led.h"
#include "esp_log.h"
#include "esp_timer.h" // Added for restart timer
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mgr_ir_protocols.h"
#include "sdkconfig.h"
#include "svc_nvs.h"
// #include "ir_encoder.h" // Removed external dependency
#include "drv_ir_rmt.h" // Added for ir_engine_init
#include "ir_engine.h"
#include <esp_rom_sys.h> // For ets_printf
#include <inttypes.h>
#include <string.h>

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

static rmt_channel_handle_t s_rx_channel = NULL;
// static rmt_channel_handle_t s_tx_channel = NULL;
// static rmt_encoder_handle_t s_ir_encoder = NULL;
static esp_timer_handle_t s_restart_timer = NULL;

#include "esp_heap_caps.h" // Added for heap_caps_malloc

// Data storage
#define MAX_IR_SYMBOLS 600 // Safe size for DMA (Max < 4095 bytes)

<<<<<<< HEAD
// Dynamic buffer for learning (Made non-static for Matrix access)
rmt_symbol_word_t *s_learning_symbols = NULL;
uint32_t s_learning_num_symbols = 0;
=======
// Dynamic buffer for learning
static rmt_symbol_word_t *s_learning_symbols = NULL;
static uint32_t s_learning_num_symbols = 0;
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
static bool s_is_learning = false;
static bool s_is_slave_mode = false;
static mgr_ir_rx_cb_t s_rx_callback = NULL;

// Forward declaration
static void mgr_ir_restart_reception(void *arg);

void mgr_ir_set_rx_callback(mgr_ir_rx_cb_t cb) { s_rx_callback = cb; }

// --- Memory Helper ---
static void *mgr_ir_malloc(size_t size) {
  // Try PSRAM first (if configured and available)
  // MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT handles external RAM
  void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ptr == NULL) {
    // Fallback to internal memory
    ptr = malloc(size);
  }
  return ptr;
}

// --- Palette-based Compression ---

#define IR_DATA_MAGIC 0xA5
#define PALETTE_TOLERANCE 100 // 100us tolerance for grouping

typedef struct {
  uint16_t duration;
} ir_palette_item_t;

// Find or add duration to palette
static int app_ir_get_palette_idx(uint16_t duration, ir_palette_item_t *palette,
                                  uint8_t *palette_size, uint8_t max_palette) {
  for (int i = 0; i < *palette_size; i++) {
    if (abs((int)palette[i].duration - (int)duration) <= PALETTE_TOLERANCE) {
      return i;
    }
  }
  if (*palette_size < max_palette) {
    palette[*palette_size].duration = duration;
    (*palette_size)++;
    return (*palette_size) - 1;
  }
  return -1; // Palette full
}

/**
 * @brief Encode IR symbols using Palette-based compression (4-bit nibbles)
 * Format: [Magic:1][Count:4][PalSize:1][Palette:P*2][Data: ceil(N/2)]
 */
<<<<<<< HEAD
static __attribute__((unused)) size_t app_ir_encode(const rmt_symbol_word_t *src, uint32_t count,
=======
static size_t app_ir_encode(const rmt_symbol_word_t *src, uint32_t count,
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
                            uint8_t *dst, size_t max_len) {
  if (count == 0)
    return 0;

  ir_palette_item_t palette[16];
  uint8_t palette_size = 0;
  uint8_t *indices = (uint8_t *)mgr_ir_malloc(count); // Use mgr_ir_malloc
  if (!indices)
    return 0;

  // 1. Build Palette and Indices
  const uint16_t *src_items = (const uint16_t *)src; // Treat as u16 stream
  bool success = true;

  for (uint32_t i = 0; i < count; i++) {
    uint16_t val = src_items[i];
    uint16_t duration = val & 0x7FFF; // Ignore level for quantization

    int idx = app_ir_get_palette_idx(duration, palette, &palette_size, 16);
    if (idx < 0) {
      // Fallback: If palette full, maybe too complex signal?
      // For AC, 16 distinct durations is usually enough (Header, 0, 1, Repeat,
      // End). If failed, we could fallback to raw or 8-bit, but 16 should cover
      // 99% ACs.
      ESP_LOGW(TAG, "IR Compression: Palette overflow (>16 unique durations)");
      success = false;
      break;
    }
    indices[i] = (uint8_t)idx;
  }

  if (!success) {
    free(indices);
    return 0;
  }

  // 2. Calculate Size
  size_t data_len = (count + 1) / 2; // Packed 4-bit
  size_t total_len = 1 + 4 + 1 + (palette_size * 2) + data_len;

  if (total_len > max_len) {
    free(indices);
    return 0; // Buffer too small
  }

  // 3. Serialize
  if (dst) {
    uint8_t *p = dst;
    *p++ = IR_DATA_MAGIC;
    memcpy(p, &count, 4);
    p += 4;
    *p++ = palette_size;
    for (int i = 0; i < palette_size; i++) {
      memcpy(p, &palette[i].duration, 2);
      p += 2;
    }

    memset(p, 0, data_len);
    for (uint32_t i = 0; i < count; i++) {
      if (i % 2 == 0) {
        p[i / 2] |= (indices[i] & 0x0F) << 4; // High nibble
      } else {
        p[i / 2] |= (indices[i] & 0x0F); // Low nibble
      }
    }
  }

  free(indices);
  return total_len;
}

static size_t app_ir_decode(const uint8_t *src, size_t src_len,
                            rmt_symbol_word_t *dst, size_t dst_max_bytes) {
  if (src_len < 6)
    return 0; // Header size

  const uint8_t *p = src;
  if (*p++ != IR_DATA_MAGIC)
    return 0; // Invalid Magic

  uint32_t count;
  memcpy(&count, p, 4);
  p += 4;
  uint8_t palette_size = *p++;

  if (src_len < (6 + palette_size * 2))
    return 0;

  // Read Palette
  uint16_t palette[16];
  for (int i = 0; i < palette_size; i++) {
    memcpy(&palette[i], p, 2);
    p += 2;
  }

  // Check output buffer
  size_t required_dst_bytes = count * sizeof(uint16_t);
  // Ensure we unpack into rmt_symbol_word_t correctly (which is what dst points
  // to) But wait, the caller usually expects rmt_symbol_word_t array. We need
  // to verify if dst_max_bytes matches.
  if (required_dst_bytes > dst_max_bytes)
    return 0;

  // Unpack
  // C3/S3: rmt_symbol_word_t is 32-bit (2 items). We treat dst as uint16_t*
  // stream.
  uint16_t *dst_u16 = (uint16_t *)dst;

  int data_idx = 0;
  for (uint32_t i = 0; i < count; i++) {
    uint8_t idx;
    if (i % 2 == 0) {
      idx = (p[data_idx] >> 4) & 0x0F;
    } else {
      idx = (p[data_idx] & 0x0F);
      data_idx++;
    }

    if (idx >= palette_size)
      idx = 0; // Correction

    uint16_t duration = palette[idx];
    uint16_t level =
        (i % 2 == 0) ? 1 : 0; // Reconstruct implicit level: Mark, Space...

    // Wait! Raw capture might not start with Mark=1 always, but usually yes for
    // IR. However, the original capture had levels. Optimisation: We assumed
    // strict alternation. If capture was garbage check? For now, standard IR is
    // Modulated(1) then Space(0).

    dst_u16[i] = duration | (level << 15);
  }

  return count; // Return number of symbols decoded
}

// RX Callback — runs in ISR context on the RMT interrupt.
// CRITICAL ISR RULES:
//   1. No heap alloc (malloc/free) — use static or pre-allocated buffers only.
//   2. No ESP_LOG* — routes through vfprintf which blows the ISR stack.
//      Use esp_rom_printf() for minimal safe output.
//   3. No blocking calls (semaphore take with timeout, vTaskDelay, etc.).
//   4. Use drv_led_set_state_isr() NOT drv_led_set_state() — the latter
//      triggers the ESP-NOW bridge callback which calls ESP_LOGI -> crash.
static bool app_ir_rx_done_callback(rmt_channel_handle_t rx_chan,
                                    const rmt_rx_done_event_data_t *edata,
                                    void *user_ctx) {
  if (s_is_learning || s_is_slave_mode) {
    if (edata->num_symbols < APP_IR_MIN_SYMBOLS) {
      if (s_is_learning) {
        esp_rom_printf("IR Noise: %d symbols. Relearning...\n",
                       (int)edata->num_symbols);
        // ISR-safe: no bridge callback, no ESP_LOGI
        drv_led_set_state_isr(DRV_LED_IR_FAIL);
      }
      esp_timer_start_once(s_restart_timer, 500000); // 500ms delay
    } else {
      // Valid data received — copy symbol count only (buffer is DMA, already
      // written by hardware, no copy needed here).
      uint32_t num_symbols = (edata->num_symbols > MAX_IR_SYMBOLS)
                                 ? MAX_IR_SYMBOLS
                                 : edata->num_symbols;

      if (s_is_learning) {
        s_learning_num_symbols = num_symbols;
        esp_rom_printf("IR RX Valid! Symbols: %d\n",
                       (int)s_learning_num_symbols);
        s_is_learning = false;
        // ISR-safe: only update the state variable, bridge fires later in
        // mgr_ir_restart_reception() timer callback (normal task context).
        drv_led_set_state_isr(DRV_LED_IR_SUCCESS);
        esp_timer_start_once(s_restart_timer, 1000000); // 1s delay
      } else if (s_is_slave_mode && s_rx_callback) {
        // Slave mode: schedule callback via timer (normal context).
        // Avoid any blocking or logging here.
        esp_timer_start_once(s_restart_timer, 100000); // 100ms delay
      }
    }
  }
  return false;
}

static void mgr_ir_restart_reception(void *arg) {
  // This runs in esp_timer task context (normal, not ISR) — safe to:
  // malloc, ESP_LOG*, call s_rx_callback, call drv_led_set_state().

  if (!s_is_learning && !s_is_slave_mode) {
    // Learn was stopped or completed — just return to idle LED state.
    // drv_led_set_state() triggers the bridge callback safely here.
    drv_led_set_state(DRV_LED_IDLE);
    return;
  }
  if (!s_learning_symbols)
    return;

  // Slave mode: the ISR deferred the s_rx_callback here to avoid ISR stack
  // overflow. Invoke it now with the DMA buffer data (safe in timer context).
  if (s_is_slave_mode && s_rx_callback && s_learning_num_symbols > 0) {
    uint32_t logical_count =
        s_learning_num_symbols * (sizeof(rmt_symbol_word_t) / sizeof(uint16_t));
    uint16_t *durations = (uint16_t *)malloc(logical_count * sizeof(uint16_t));
    if (durations) {
      const uint16_t *raw_symbols = (const uint16_t *)s_learning_symbols;
      for (uint32_t i = 0; i < logical_count; i++) {
        durations[i] = raw_symbols[i] & 0x7FFF;
      }
      s_rx_callback(durations, logical_count);
      free(durations);
    }
    s_learning_num_symbols = 0; // Reset for next capture
  }

  rmt_receive_config_t receive_config = {
      .signal_range_min_ns = 1250,
      .signal_range_max_ns = 30000000, // 30ms
  };

  if (s_is_learning) {
    drv_led_set_state(DRV_LED_IR_LEARN);
  }

  esp_err_t err =
      rmt_receive(s_rx_channel, s_learning_symbols,
                  MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t), &receive_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_receive failed: %s", esp_err_to_name(err));
    drv_led_set_state(DRV_LED_IR_FAIL);
    s_is_learning = false;
  }
}

esp_err_t mgr_ir_init(void) {
  if (IR_RX_GPIO == -1 || IR_TX_GPIO == -1) {
    ESP_LOGI(TAG, "IR GPIOs not configured (Master mode), skipping IR init");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing IR RX on GPIO %d, TX on GPIO %d", IR_RX_GPIO,
           IR_TX_GPIO);

  // Alloc Learning Buffer
  // RMT driver requires internal RAM for receive buffer even with DMA on some
  // targets/versions
  s_learning_symbols = (rmt_symbol_word_t *)heap_caps_malloc(
      MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t),
      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!s_learning_symbols) {
    ESP_LOGE(TAG, "Failed to alloc learning buffer");
    return ESP_ERR_NO_MEM;
  }

  // 1. Initialize RX Channel
  ESP_LOGI(TAG, "Initializing RX Channel on GPIO %d...", IR_RX_GPIO);
  rmt_rx_channel_config_t rx_chan_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = RMT_RESOLUTION_HZ,
      .mem_block_symbols = 64, // 64 symbols per channel
      .gpio_num = IR_RX_GPIO,
      .flags.invert_in = 1, // Active Low
#if defined(CONFIG_IDF_TARGET_ESP32S3)
      .flags.with_dma = 1, // Enable DMA for large buffers on capable chips
#endif
  };
  ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_chan_config, &s_rx_channel));

  // CRITICAL: IR Receiver requires Pull-Up (Probe worked because it enabled it)
  gpio_set_pull_mode(IR_RX_GPIO, GPIO_PULLUP_ONLY);

  // Verify RX channel creation
  if (!s_rx_channel) {
    ESP_LOGE(TAG, "Failed to create RX channel");
    return ESP_FAIL;
  }

  // Register RX callbacks
  rmt_rx_event_callbacks_t cbs = {
      .on_recv_done = app_ir_rx_done_callback,
  };
  ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(s_rx_channel, &cbs, NULL));
  ESP_ERROR_CHECK(rmt_enable(s_rx_channel));

  // 2. Initialize TX Channel (USING IR ENGINE)
  ir_engine_config_t engine_cfg = {
      .gpio_num = IR_TX_GPIO,
      .resolution_hz = RMT_RESOLUTION_HZ,
  };
  ESP_ERROR_CHECK(ir_engine_init(&engine_cfg));

  // 3. (Old Encoder init removed)

  // 4. Initialize Restart Timer
  esp_timer_create_args_t timer_args = {
      .callback = mgr_ir_restart_reception,
      .name = "ir_restart",
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_restart_timer));

  ESP_LOGI(TAG, "IR Initialized (Native RMT + Copy Encoder)");
  return ESP_OK;
}

esp_err_t mgr_ir_start_slave(void) {
  if (!s_rx_channel)
    return ESP_ERR_INVALID_STATE;

  ESP_LOGI(TAG, "Starting IR Slave Mode (Background RX)...");
  s_is_slave_mode = true;
  mgr_ir_restart_reception(NULL);
  return ESP_OK;
}

esp_err_t mgr_ir_start_learn(void) {
  if (!s_rx_channel)
    return ESP_ERR_INVALID_STATE;

  ESP_LOGI(TAG, "Starting IR Learn...");

  // Cancel any pending restart timer from a previous learn session.
  // Without this, the old 1s timer fires after the new rmt_receive() is
  // already running and calls rmt_receive() again -> ESP_ERR_INVALID_STATE.
  esp_timer_stop(s_restart_timer); // Safe to call even if not running.

  // Disable+re-enable channel to reset it to a clean READY state.
  // Required if a previous learn completed (channel is in IDLE/done state
  // after the last receive finished) or was aborted mid-receive.
  rmt_disable(s_rx_channel);
  rmt_enable(s_rx_channel);

  s_is_learning = true;
  s_learning_num_symbols = 0;
  drv_led_set_state(DRV_LED_IR_LEARN);

  mgr_ir_restart_reception(NULL);
  return ESP_OK;
}

esp_err_t mgr_ir_stop_learn(void) {
  ESP_LOGI(TAG, "Stopping IR Learn...");

  s_is_learning = false;

  // Cancel the pending restart timer. If we don't do this, the timer fires
  // and calls rmt_receive() when mgr_ir_start_learn() has already called it
  // via the disable+enable+restart path -> ESP_ERR_INVALID_STATE.
  esp_timer_stop(s_restart_timer);

  // Disable the RMT RX channel. This aborts any in-progress receive and puts
  // the channel back to the INIT state, ready for the next enable+receive.
  rmt_disable(s_rx_channel);

  drv_led_set_state(DRV_LED_IDLE);
  return ESP_OK;
}

// Check if data was received (polled by main loop or web handler)
bool mgr_ir_is_data_ready(void) {
  return (!s_is_learning && s_learning_num_symbols >= APP_IR_MIN_SYMBOLS);
}

bool mgr_ir_get_learn_status(uint32_t *count) {
  if (count)
    *count = s_learning_num_symbols;
  return s_is_learning;
}

#define IR_RAW_MAGIC 0xAA

#include <ctype.h>

static void normalize_key(char *dst, const char *src, size_t max_len) {
  size_t i = 0;
  for (; i < max_len - 1 && src[i]; i++) {
    dst[i] = toupper((unsigned char)src[i]);
  }
  dst[i] = '\0'; // Explicit null termination
}

esp_err_t mgr_ir_save_learned_result(const char *key) {
  char upper_key[32] = {0};
  normalize_key(upper_key, key, sizeof(upper_key));

  if (!s_learning_symbols || s_learning_num_symbols < APP_IR_MIN_SYMBOLS) {
    ESP_LOGW(TAG, "Insufficient IR data symbols to save for %s", upper_key);
    return ESP_FAIL;
  }

  // Calculate total logical symbols (each rmt_symbol_word_t contains 2
  // uint16_t)
  uint32_t logical_count = s_learning_num_symbols * 2;

  // Format: [Magic:1][Count:4][Data: N*2]
  size_t total_size = 1 + 4 + (logical_count * sizeof(uint16_t));
  uint8_t *buffer = (uint8_t *)mgr_ir_malloc(total_size);
  if (!buffer)
    return ESP_ERR_NO_MEM;

  uint8_t *p = buffer;
  *p++ = IR_RAW_MAGIC;
  memcpy(p, &logical_count, 4);
  p += 4;
  memcpy(p, s_learning_symbols, logical_count * sizeof(uint16_t));

  ESP_LOGI(TAG, "Saving RAW IR %s: %d symbols -> %d bytes", upper_key,
           (int)logical_count, (int)total_size);
  esp_err_t err = svc_nvs_save_ir(upper_key, buffer, total_size);

  // Debug print
  printf("RAW DATA: ");
  uint16_t *raw_ptr = (uint16_t *)s_learning_symbols;
  for (int i = 0; i < (int)logical_count && i < 32; i++)
    printf("%04X ", raw_ptr[i]);
  printf("...\n");

  free(buffer);
  return err;
}

esp_err_t mgr_ir_send_key(const char *key) {
  char upper_key[32] = {0};
  normalize_key(upper_key, key, sizeof(upper_key));

  size_t loaded_size = 0;
  if (svc_nvs_load_ir(upper_key, NULL, &loaded_size) != ESP_OK ||
      loaded_size < 5) {
    ESP_LOGW(TAG, "Key %s not found in NVS", upper_key);
    return ESP_ERR_NOT_FOUND;
  }

  uint8_t *buffer = (uint8_t *)mgr_ir_malloc(loaded_size);
  if (!buffer)
    return ESP_ERR_NO_MEM;
  svc_nvs_load_ir(upper_key, buffer, &loaded_size);

  uint8_t format = buffer[0];
  uint32_t num_symbols = 0;
  memcpy(&num_symbols, buffer + 1, 4);

  size_t alloc_size = num_symbols * sizeof(uint16_t);
  if (alloc_size % 4 != 0)
    alloc_size += 2; // Align to rmt_symbol_word_t (4 bytes)
  rmt_symbol_word_t *tx_symbols =
      (rmt_symbol_word_t *)mgr_ir_malloc(alloc_size);

  if (!tx_symbols) {
    free(buffer);
    return ESP_ERR_NO_MEM;
  }
  // Zero-init: ensures padding half of last word is duration=0, level=0
  // (end-of-frame)
  memset(tx_symbols, 0, alloc_size);

  if (format == IR_RAW_MAGIC) {
    // RX channel has invert_in=1, so stored levels are already TX-correct:
    //   level=1 = Mark (IR carrier ON),  level=0 = Space (silence)
    // NO inversion needed. Direct copy, only skip leading spaces.
    uint16_t *src = (uint16_t *)(buffer + 5);
    uint16_t *dst = (uint16_t *)tx_symbols;
    int out_idx = 0;

    for (int i = 0; i < (int)num_symbols; i++) {
      uint16_t val = src[i];
      uint16_t level = (val >> 15) & 0x1;

      // Skip leading spaces (level=0) at the start
      if (out_idx == 0 && level == 0)
        continue;

      dst[out_idx++] = val; // Copy as-is, levels are already correct
    }
    num_symbols = out_idx;

    // Strip trailing zero-duration symbols (RX timeout artifacts)
    while (num_symbols > 0 && (dst[num_symbols - 1] & 0x7FFF) == 0) {
      num_symbols--;
    }

    // Ensure signal ends with odd count (last = mark, no trailing space)
    // Most IR protocols end with a final mark pulse
    if (num_symbols > 0 && num_symbols % 2 == 0) {
      uint16_t last = dst[num_symbols - 1];
      if ((last >> 15) == 0) { // Last is a space — drop it
        num_symbols--;
      }
    }

    // Debug: print TX symbols
    printf("TX [%d]: ", (int)num_symbols);
    for (int i = 0; i < (int)num_symbols; i++) {
      uint16_t v = dst[i];
      printf("%c%d ", (v & 0x8000) ? 'M' : 'S', v & 0x7FFF);
    }
    printf("\n");
  } else if (format == IR_DATA_MAGIC) {
    app_ir_decode(buffer, loaded_size, tx_symbols, alloc_size);
  } else {
    ESP_LOGE(TAG, "Unknown IR format: 0x%02X", format);
    free(buffer);
    free(tx_symbols);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Sending IR %s (%d symbols, Format: 0x%02X)...", upper_key,
           (int)num_symbols, format);
  drv_led_set_state(DRV_LED_IR_TX);

  size_t word_count = (num_symbols + 1) / 2;

  // Send 3 times with gap — fan receivers often need repeated frames
  esp_err_t err = ESP_OK;
  for (int rep = 0; rep < 2 && err == ESP_OK; rep++) {
    err = ir_engine_send_raw(tx_symbols, word_count);
    if (rep < 2 && err == ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(40));
    }
  }

  drv_led_set_state(DRV_LED_IDLE);
  free(tx_symbols);
  free(buffer);
  return err;
}

esp_err_t mgr_ir_send_raw(const uint16_t *durations, size_t count) {
  // if (!s_tx_channel || !s_ir_encoder)
  //    return ESP_ERR_INVALID_STATE;

  if (count == 0 || durations == NULL)
    return ESP_ERR_INVALID_ARG;

  // Allocate RMT symbols
  // Allocate RMT symbols
  size_t alloc_size = (count * sizeof(uint16_t));
  if (alloc_size % 4 != 0)
    alloc_size += 2;

  rmt_symbol_word_t *tx_symbols =
      (rmt_symbol_word_t *)mgr_ir_malloc(alloc_size);
  if (!tx_symbols) {
    ESP_LOGE(TAG, "Failed to allocate memory for %d raw symbols", (int)count);
    return ESP_ERR_NO_MEM;
  }

  uint16_t *tx_raw = (uint16_t *)tx_symbols;

  // Convert uS durations to RMT symbols
  // IRremoteESP8266 format: [puls, space, pulse, space, ...]
  for (size_t i = 0; i < count; i++) {
    uint16_t level = (i % 2 == 0) ? 1 : 0; // Starts with Pulse (Mark)
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

  // rmt_transmit_config_t tx_config = {.loop_count = 0};
  // ESP_ERROR_CHECK(rmt_transmit(s_tx_channel, s_ir_encoder, tx_symbols,
  //                              count * sizeof(uint16_t), &tx_config));
  // ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_tx_channel, -1));

  drv_led_set_state(DRV_LED_IDLE);
  free(tx_symbols);
  return ESP_OK;
}

esp_err_t mgr_ir_send_cmd(mgr_ir_cmd_t cmd) {
  return mgr_ir_send_key((cmd == APP_IR_CMD_AC_ON) ? "ac_on" : "ac_off");
}

bool mgr_ir_send_key_exists(const char *prefix, const char *brand,
                            const char *suffix) {
<<<<<<< HEAD
  char short_brand[16] = {0};
  int idx = 0;
  for (int i = 0; brand[i] && idx < 15; i++) {
=======
  char short_brand[9] = {0};
  int idx = 0;
  for (int i = 0; brand[i] && idx < 8; i++) {
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
    char c = brand[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
      short_brand[idx++] = c;
    }
  }

<<<<<<< HEAD
  char key[32];
=======
  char key[16];
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
  snprintf(key, sizeof(key), "%s%s_%s", prefix, short_brand, suffix);

  // Normalize to uppercase — save path uses normalize_key (uppercase),
  // so lookup must match.
<<<<<<< HEAD
  char upper_key[32] = {0};
=======
  char upper_key[16] = {0};
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
  normalize_key(upper_key, key, sizeof(upper_key));

  size_t loaded_size = 0;
  if (svc_nvs_load_ir(upper_key, NULL, &loaded_size) == ESP_OK &&
      loaded_size > 0) {
    return true;
  }
  return false;
}
