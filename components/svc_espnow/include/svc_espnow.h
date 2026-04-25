#include "drv_ir_rmt.h"
#include "esp_err.h"

typedef enum {
    ESPNOW_TYPE_AC = 0,
    ESPNOW_TYPE_LED = 1,
    ESPNOW_TYPE_FAN = 2,
    ESPNOW_TYPE_TEMP = 3,
    ESPNOW_TYPE_RELAY = 4
} espnow_msg_type_t;

/**
 * @brief Initialize ESP-NOW service.
 */
esp_err_t svc_espnow_init(void);

// --- Handler types (Slave receives) ---
typedef void (*espnow_ac_handler_t)(const ir_ac_state_t *state, ac_brand_t brand, 
                                  const char *custom_name);
typedef void (*espnow_led_handler_t)(uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness,
                                    uint8_t r, uint8_t g, uint8_t b, uint8_t speed);
typedef void (*espnow_fan_handler_t)(const ir_fan_state_t *state, fan_brand_t brand, 
                                   const char *custom_name);
typedef void (*espnow_temp_handler_t)(float temperature, float humidity);
typedef void (*espnow_relay_handler_t)(uint8_t idx, bool state);

// --- Register handlers (Slave/Master receive side) ---
void svc_espnow_register_ac_handler(espnow_ac_handler_t handler);
void svc_espnow_register_led_handler(espnow_led_handler_t handler);
void svc_espnow_register_fan_handler(espnow_fan_handler_t handler);
void svc_espnow_register_temp_handler(espnow_temp_handler_t handler);
void svc_espnow_register_relay_handler(espnow_relay_handler_t handler);

// --- Bridge send functions ---
esp_err_t svc_espnow_bridge_ac_send(const ir_ac_state_t *state, ac_brand_t brand, 
                                  const char *custom_name);

esp_err_t svc_espnow_bridge_led_send(uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness,
                                    uint8_t r, uint8_t g, uint8_t b, uint8_t speed);

esp_err_t svc_espnow_bridge_led_send_to(const uint8_t *mac, uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t speed);

esp_err_t svc_espnow_bridge_fan_send(const ir_fan_state_t *state, fan_brand_t brand, 
                                   const char *custom_name);

esp_err_t svc_espnow_bridge_temp_send(float temperature, float humidity);
esp_err_t svc_espnow_bridge_relay_send(uint8_t idx, bool state);

// --- Peer Management ---
esp_err_t svc_espnow_add_peer(const uint8_t *mac);
esp_err_t svc_espnow_remove_peer(const uint8_t *mac);
esp_err_t svc_espnow_get_peers(uint8_t *mac_list, int *count);
