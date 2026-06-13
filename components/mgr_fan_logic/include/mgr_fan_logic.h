#ifndef MGR_FAN_LOGIC_H
#define MGR_FAN_LOGIC_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#include "ir_types.h"

typedef void (*mgr_fan_bridge_cb_t)(const ir_fan_state_t *state, fan_brand_t brand, const char *custom_name);

void mgr_fan_init(void);
void mgr_fan_set_state(const ir_fan_state_t *state);
void mgr_fan_get_state(ir_fan_state_t *state);
void mgr_fan_set_brand(fan_brand_t brand);
fan_brand_t mgr_fan_get_brand(void);
void mgr_fan_set_custom_brand(const char *name);
const char *mgr_fan_get_custom_brand(void);
bool mgr_fan_is_custom_brand(void);

void mgr_fan_set_bridge_cb(mgr_fan_bridge_cb_t cb);
esp_err_t mgr_fan_send(void);
bool mgr_fan_is_configured(void);

#endif
