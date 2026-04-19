/**
 * @file mgr_ac_logic.h
 * @brief AC Control Logic
 */

#pragma once

#include "drv_ir_rmt.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Enum defined in ir_types.h included via ir_engine.h

/**
 * @brief Callback for bridging AC state to other services (e.g. ESP-NOW)
 */
typedef void (*mgr_ac_bridge_cb_t)(const ir_ac_state_t *state, ac_brand_t brand, 
                                 const char *custom_name);

void mgr_ac_set_bridge_cb(mgr_ac_bridge_cb_t cb);

/**
 * @brief Initialize AC logic (load state from NVS if needed)
 */
void mgr_ac_init(void);

/**
 * @brief Set the full AC state.
 *
 * @param state Pointer to new state
 */
void mgr_ac_set_state(const ir_ac_state_t *state);

/**
 * @brief Get the current AC state.
 *
 * @param state Pointer to store state
 */
void mgr_ac_get_state(ir_ac_state_t *state);

/**
 * @brief Set the AC brand.
 *
 * @param brand New brand
 */
void mgr_ac_set_brand(ac_brand_t brand);

/**
 * @brief Get the current AC brand.
 *
 * @return ac_brand_t Current brand
 */
ac_brand_t mgr_ac_get_brand(void);

/**
 * @brief Send the IR command based on current state and brand.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_ac_send(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief Set custom AC brand by name.
 *
 * @param name Custom brand name
 */
void mgr_ac_set_custom_brand(const char *name);

/**
 * @brief Get current custom brand name.
 *
 * @return const char* Custom brand name or NULL if not custom
 */
const char *mgr_ac_get_custom_brand(void);

/**
 * @brief Check if current brand is custom.
 *
 * @return bool True if custom brand is active
 */
bool mgr_ac_is_custom_brand(void);
