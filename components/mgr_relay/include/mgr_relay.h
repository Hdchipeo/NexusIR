#ifndef MGR_RELAY_H
#define MGR_RELAY_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef void (*mgr_relay_bridge_cb_t)(uint8_t idx, bool state);

/**
 * @brief Initialize the Dual Touch Relay module
 */
esp_err_t mgr_relay_init(void);

/**
 * @brief Set relay bridge callback
 */
void mgr_relay_set_bridge_cb(mgr_relay_bridge_cb_t cb);

/**
 * @brief Set relay state
 * 
 * @param relay_idx 0 or 1
 * @param state true for ON, false for OFF
 * @param report_to_homekit if true, update HomeKit characteristic
 */
void mgr_relay_set_state(int relay_idx, bool state, bool report_to_homekit);

/**
 * @brief Get relay state
 * 
 * @param relay_idx 0 or 1
 * @return true if ON, false if OFF
 */
bool mgr_relay_get_state(int relay_idx);

#endif // MGR_RELAY_H
