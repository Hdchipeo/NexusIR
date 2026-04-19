#include "drv_ir_rmt.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
// #include "ir_engine.h" // Removed legacy header dependency if possible

static const char *TAG = "drv_ir_rmt";

static rmt_channel_handle_t g_tx_channel = NULL;
static rmt_encoder_handle_t g_copy_encoder = NULL;

extern "C" esp_err_t ir_engine_init(const ir_engine_config_t *config) {
  if (!config)
    return ESP_ERR_INVALID_ARG;

  ESP_LOGI(TAG, "Initializing IR Engine on GPIO %d", config->gpio_num);

  rmt_tx_channel_config_t tx_chan_config = {
      .gpio_num = (gpio_num_t)config->gpio_num,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = (uint32_t)config->resolution_hz,
      .mem_block_symbols = 64,
      .trans_queue_depth = 4,
      .intr_priority = 0,
      .flags = 0,
  };

  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &g_tx_channel));

  rmt_carrier_config_t carrier_cfg = {
      .frequency_hz = 38000,
      .duty_cycle = 0.33,
      .flags = 0,
  };
  ESP_ERROR_CHECK(rmt_apply_carrier(g_tx_channel, &carrier_cfg));
  ESP_ERROR_CHECK(rmt_enable(g_tx_channel));

  // Create Copy Encoder (Standard IDF)
  rmt_copy_encoder_config_t copy_encoder_config = {};
  ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &g_copy_encoder));

  return ESP_OK;
}

extern "C" esp_err_t ir_engine_send_raw(const void *symbols, size_t count) {
  if (!g_tx_channel || !g_copy_encoder) {
    return ESP_ERR_INVALID_STATE;
  }

  rmt_transmit_config_t tx_config = {.loop_count = 0, .flags = 0};
  ESP_ERROR_CHECK(rmt_transmit(g_tx_channel, g_copy_encoder, symbols,
                               count * sizeof(rmt_symbol_word_t), &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(g_tx_channel, -1));

  return ESP_OK;
}
