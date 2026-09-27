/**
 * @file drv_ir_rmt.cpp
 * @brief IR RMT Driver (Hardware Abstraction Layer for TX & RX)
 */

#include "drv_ir_rmt.h"
#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "drv_ir_rmt";

// Hardware handles
static rmt_channel_handle_t g_tx_channel = NULL;
static rmt_encoder_handle_t g_copy_encoder = NULL;
static rmt_channel_handle_t g_rx_channel = NULL;

// Carrier state cache
static uint32_t g_current_carrier_hz = 38000;
static float g_current_duty_cycle = 0.33f;

// RX state
static bool g_rx_enabled = false;
static ir_rx_done_isr_callback_t g_user_rx_done_cb = NULL;
static void *g_user_rx_ctx = NULL;

static bool rmt_rx_done_internal_cb(rmt_channel_handle_t rx_chan,
                                    const rmt_rx_done_event_data_t *edata,
                                    void *user_ctx) {
  if (g_user_rx_done_cb) {
    return g_user_rx_done_cb(edata->num_symbols, g_user_rx_ctx);
  }
  return false;
}

extern "C" esp_err_t ir_engine_init(const ir_engine_config_t *config) {
  if (!config) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "Initializing IR TX Engine on GPIO %d (Resolution: %d Hz)",
           config->gpio_num, config->resolution_hz);

  rmt_tx_channel_config_t tx_chan_config = {};
  tx_chan_config.gpio_num = (gpio_num_t)config->gpio_num;
  tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
  tx_chan_config.resolution_hz = (uint32_t)config->resolution_hz;
  tx_chan_config.mem_block_symbols = 64;
  tx_chan_config.trans_queue_depth = 4;
  tx_chan_config.intr_priority = 0;

  esp_err_t err = rmt_new_tx_channel(&tx_chan_config, &g_tx_channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create TX channel: %s", esp_err_to_name(err));
    return err;
  }

  rmt_carrier_config_t carrier_cfg = {};
  carrier_cfg.frequency_hz = g_current_carrier_hz;
  carrier_cfg.duty_cycle = g_current_duty_cycle;
  err = rmt_apply_carrier(g_tx_channel, &carrier_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to apply carrier: %s", esp_err_to_name(err));
    return err;
  }

  err = rmt_enable(g_tx_channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable TX channel: %s", esp_err_to_name(err));
    return err;
  }

  rmt_copy_encoder_config_t copy_encoder_config = {};
  err = rmt_new_copy_encoder(&copy_encoder_config, &g_copy_encoder);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create copy encoder: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "IR TX Engine initialized successfully");
  return ESP_OK;
}

extern "C" esp_err_t ir_engine_set_carrier(uint32_t freq_hz, float duty_cycle) {
  if (!g_tx_channel) {
    return ESP_ERR_INVALID_STATE;
  }
  if (freq_hz < 10000 || freq_hz > 100000) {
    ESP_LOGW(TAG, "Invalid carrier frequency requested: %" PRIu32 " Hz", freq_hz);
    return ESP_ERR_INVALID_ARG;
  }
  if (duty_cycle <= 0.05f || duty_cycle >= 0.95f) {
    ESP_LOGW(TAG, "Invalid carrier duty cycle requested: %f", duty_cycle);
    return ESP_ERR_INVALID_ARG;
  }

  // Fast path: Avoid hardware reconfiguration if identical
  if (g_current_carrier_hz == freq_hz &&
      (int)(g_current_duty_cycle * 100) == (int)(duty_cycle * 100)) {
    return ESP_OK;
  }

  rmt_carrier_config_t carrier_cfg = {};
  carrier_cfg.frequency_hz = freq_hz;
  carrier_cfg.duty_cycle = duty_cycle;
  esp_err_t err = rmt_apply_carrier(g_tx_channel, &carrier_cfg);
  if (err == ESP_OK) {
    g_current_carrier_hz = freq_hz;
    g_current_duty_cycle = duty_cycle;
    ESP_LOGI(TAG, "Carrier updated: %" PRIu32 " Hz (Duty: %d%%)",
             freq_hz, (int)(duty_cycle * 100));
  } else {
    ESP_LOGE(TAG, "Failed to update carrier: %s", esp_err_to_name(err));
  }
  return err;
}

extern "C" esp_err_t ir_engine_send_raw(const void *symbols, size_t count) {
  if (!g_tx_channel || !g_copy_encoder || !symbols || count == 0) {
    return ESP_ERR_INVALID_STATE;
  }

  rmt_transmit_config_t tx_config = {};
  tx_config.loop_count = 0;

  esp_err_t err = rmt_transmit(g_tx_channel, g_copy_encoder, symbols,
                               count * sizeof(rmt_symbol_word_t), &tx_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
    return err;
  }

  // 3-second safety timeout prevents task hangs in 24/7 operation
  err = rmt_tx_wait_all_done(g_tx_channel, pdMS_TO_TICKS(3000));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_tx_wait_all_done timeout or failed: %s", esp_err_to_name(err));
  }
  return err;
}

extern "C" esp_err_t ir_engine_rx_init(const ir_rx_engine_config_t *config) {
  if (!config) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "Initializing IR RX Engine on GPIO %d (Resolution: %d Hz)",
           config->gpio_num, (int)config->resolution_hz);

  g_user_rx_done_cb = config->rx_done_cb;
  g_user_rx_ctx = config->user_ctx;

  rmt_rx_channel_config_t rx_chan_config = {};
  rx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
  rx_chan_config.resolution_hz = config->resolution_hz;
  rx_chan_config.mem_block_symbols = config->mem_block_symbols ? config->mem_block_symbols : 64;
  rx_chan_config.gpio_num = (gpio_num_t)config->gpio_num;
  rx_chan_config.flags.invert_in = 1;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  rx_chan_config.flags.with_dma = 1;
#endif

  esp_err_t err = rmt_new_rx_channel(&rx_chan_config, &g_rx_channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create RX channel: %s", esp_err_to_name(err));
    return err;
  }

  gpio_set_pull_mode((gpio_num_t)config->gpio_num, GPIO_PULLUP_ONLY);

  rmt_rx_event_callbacks_t cbs = {};
  cbs.on_recv_done = rmt_rx_done_internal_cb;
  err = rmt_rx_register_event_callbacks(g_rx_channel, &cbs, NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register RX event callback: %s", esp_err_to_name(err));
    return err;
  }

  err = rmt_enable(g_rx_channel);
  if (err == ESP_OK) {
    g_rx_enabled = true;
  } else {
    ESP_LOGE(TAG, "Failed to enable RX channel: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "IR RX Engine initialized successfully");
  return ESP_OK;
}

extern "C" esp_err_t ir_engine_rx_start(void *buffer, size_t buffer_size_bytes) {
  if (!g_rx_channel || !buffer || buffer_size_bytes == 0) {
    return ESP_ERR_INVALID_STATE;
  }

  if (!g_rx_enabled) {
    esp_err_t ret = rmt_enable(g_rx_channel);
    if (ret == ESP_OK) {
      g_rx_enabled = true;
    } else if (ret != ESP_ERR_INVALID_STATE) {
      return ret;
    }
  }

  rmt_receive_config_t rcfg = {};
  rcfg.signal_range_min_ns = 1250;
  rcfg.signal_range_max_ns = 30000000;
  return rmt_receive(g_rx_channel, buffer, buffer_size_bytes, &rcfg);
}

extern "C" esp_err_t ir_engine_rx_stop(void) {
  if (!g_rx_channel) {
    return ESP_ERR_INVALID_STATE;
  }
  if (g_rx_enabled) {
    esp_err_t err = rmt_disable(g_rx_channel);
    if (err == ESP_OK) {
      g_rx_enabled = false;
    }
    return (err == ESP_ERR_INVALID_STATE) ? ESP_OK : err;
  }
  return ESP_OK;
}
