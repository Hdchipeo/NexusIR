#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t int_homekit_init(void);

/**
 * @brief Check if HomeKit has any paired controllers.
 *        Initializes HAP stack if not already initialized.
 * @return true if paired, false otherwise
 */
bool int_homekit_is_paired(void);

#include "mgr_ac_logic.h"
/**
 * @brief Update HomeKit AC/Thermostat UI from current state
 * @param state Pointer to AC state
 */
void int_homekit_update_state(const ir_ac_state_t *state);

/**
 * @brief Update HomeKit LED UI from current state
 */
void int_homekit_update_led(uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b, uint8_t speed);

#include "mgr_fan_logic.h"
/**
 * @brief Update HomeKit Fan UI from current state
 * @param state Pointer to Fan state
 */
void int_homekit_update_fan_state(const ir_fan_state_t *state);

/**
 * @brief Update HomeKit Temperature/Humidity from remote sensor
 */
void int_homekit_update_temp(float temperature, float humidity);

#ifdef __cplusplus
}
#endif
