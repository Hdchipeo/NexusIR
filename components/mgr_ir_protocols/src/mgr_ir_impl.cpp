#include "drv_ir_rmt.h"
#include "esp_log.h"
#include "ir_ac_registry.hpp"
#include "ir_engine.h"
#include "ir_protocol_nec.hpp"
#include "ir_universal.hpp"

static const char *TAG = "mgr_ir_impl";

// Driver handles are managed by drv_ir_rmt, OR we pass data to it.
// Here we implement the Logic -> Protocol -> RMT Item generation.
// Then call ir_engine_send_raw from the Driver.

extern "C" esp_err_t ir_engine_send_nec(uint16_t address, uint16_t command) {

  size_t symbol_count = 0;
  rmt_symbol_word_t *symbols =
      ir_nec_generate_symbols(address, command, &symbol_count);

  if (!symbols || symbol_count == 0) {
    if (symbols)
      free(symbols);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Sending NEC: Addr=0x%04X, Cmd=0x%04X, Symbols=%d", address,
           command, (int)symbol_count);
  esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
  free(symbols);
  return err;
}

#include "ir_protocol_daikin.hpp"

extern "C" esp_err_t ir_engine_send_daikin(const ir_ac_state_t *state) {

  size_t symbol_count = 0;
  rmt_symbol_word_t *symbols = ir_daikin_generate_symbols(state, &symbol_count);

  if (!symbols)
    return ESP_FAIL;

  ESP_LOGI(TAG, "Sending Daikin AC: P=%d T=%d M=%d", state->power, state->temp,
           state->mode);

  esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
  free(symbols);
  return err;
}

#include "ir_protocol_mitsubishi.hpp"
#include "ir_protocol_samsung.hpp"

extern "C" esp_err_t ir_engine_send_samsung(const ir_ac_state_t *state) {

  size_t symbol_count = 0;
  rmt_symbol_word_t *symbols =
      ir_samsung_generate_symbols(state, &symbol_count);
  if (!symbols)
    return ESP_FAIL;
  ESP_LOGI(TAG, "Sending Samsung AC");
  esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
  free(symbols);
  return err;
}

extern "C" esp_err_t ir_engine_send_mitsubishi(const ir_ac_state_t *state) {

  size_t symbol_count = 0;
  rmt_symbol_word_t *symbols =
      ir_mitsubishi_generate_symbols(state, &symbol_count);
  if (!symbols)
    return ESP_FAIL;
  ESP_LOGI(TAG, "Sending Mitsubishi AC");
  esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
  free(symbols);
  return err;
}

extern "C" esp_err_t mgr_ir_send_ac_state(int brand, const void *state_ptr) {
  const ir_ac_state_t *state = (const ir_ac_state_t *)state_ptr;
  if (!state)
    return ESP_ERR_INVALID_STATE;

  // 1. Try Universal Registry first
  const ir_ac_definition_t *def = ir_ac_registry_get((ac_brand_t)brand);
  if (def) {
    ESP_LOGI(TAG, "Using Universal Engine for Brand %d (%s)", brand,
             def->protocol.name);

    // Translate State -> Bytes
    uint8_t payload[32] = {0}; // Reasonable max for AC
    size_t payload_len = 0;

    if (def->translator) {
      def->translator(state, payload, &payload_len);
    }

    if (payload_len == 0) {
      ESP_LOGE(TAG, "Translator failed to generate payload");
      return ESP_FAIL;
    }

    // Generate Symbols
    size_t symbol_count = 0;
    rmt_symbol_word_t *symbols = ir_universal_generate_symbols(
        &def->protocol, payload, payload_len, &symbol_count);

    if (!symbols)
      return ESP_ERR_NO_MEM;

    esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
    free(symbols);
    return err;
  }

  // 2. Fallback to Legacy Handlers
  ESP_LOGW(TAG, "Brand %d not in Registry, trying legacy...", brand);
  switch (brand) {
  case AC_BRAND_DAIKIN:
    return ir_engine_send_daikin(state);
  case AC_BRAND_MITSUBISHI:
    return ir_engine_send_mitsubishi(state);
  case AC_BRAND_SAMSUNG:
    // Should be in registry now, but keep as backup if registry lookup fails?
    return ir_engine_send_samsung(state);
  default:
    return ESP_ERR_NOT_SUPPORTED;
  }
}
