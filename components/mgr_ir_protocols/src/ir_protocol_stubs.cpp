#include "esp_log.h"
#include "ir_protocol_daikin.hpp"
#include "ir_protocol_mitsubishi.hpp"
#include "ir_protocol_samsung.hpp"

static const char *TAG = "ir_stubs";

extern "C" {

rmt_symbol_word_t *ir_daikin_generate_symbols(const ir_ac_state_t *state,
                                              size_t *out_size) {
  ESP_LOGW(TAG, "Daikin protocol not implemented (Stub)");
  if (out_size)
    *out_size = 0;
  return NULL;
}

rmt_symbol_word_t *ir_samsung_generate_symbols(const ir_ac_state_t *state,
                                               size_t *out_size) {
  ESP_LOGW(TAG, "Samsung protocol not implemented (Stub)");
  if (out_size)
    *out_size = 0;
  return NULL;
}

rmt_symbol_word_t *ir_mitsubishi_generate_symbols(const ir_ac_state_t *state,
                                                  size_t *out_size) {
  ESP_LOGW(TAG, "Mitsubishi protocol not implemented (Stub)");
  if (out_size)
    *out_size = 0;
  return NULL;
}
}
