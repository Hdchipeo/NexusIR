/**
 * @file drv_led.h
 * @brief LED Strip Control and Effects (Up to 5 Instances)
 */

#pragma once

#include <esp_err.h>
#include <stdint.h>

#define MAX_LEDS 5

/**
 * @brief LED System States
 */
typedef enum {
  DRV_LED_OFF,
  DRV_LED_STARTUP,
  DRV_LED_WIFI_PROV,
  DRV_LED_WIFI_CONN,
  DRV_LED_WIFI_SUCCESS,
  DRV_LED_WIFI_FAIL,
  DRV_LED_OTA,
  DRV_LED_IR_TX,
  DRV_LED_IR_LEARN,
  DRV_LED_IR_FAIL,
  DRV_LED_IR_SUCCESS,
  DRV_LED_IDLE,
} drv_led_state_t;

/**
 * @brief Available LED Effects
 */
typedef enum {
  DRV_LED_EFFECT_STATIC,
  DRV_LED_EFFECT_RAINBOW,
  DRV_LED_EFFECT_RUNNING,
  DRV_LED_EFFECT_BREATHING,
  DRV_LED_EFFECT_BLINK,
  DRV_LED_EFFECT_KNIGHT_RIDER,
  DRV_LED_EFFECT_LOADING,
  DRV_LED_EFFECT_COLOR_WIPE,
  DRV_LED_EFFECT_THEATER_CHASE,
  DRV_LED_EFFECT_FIRE,
  DRV_LED_EFFECT_SPARKLE,
  DRV_LED_EFFECT_RANDOM,
  DRV_LED_EFFECT_AUTO_CYCLE,
} drv_led_effect_t;

/**
 * @brief Callback for bridging LED state
 */
typedef void (*drv_led_bridge_cb_t)(uint8_t lamp_id, uint8_t power, uint8_t effect, uint8_t brightness,
                                    uint8_t r, uint8_t g, uint8_t b, uint8_t speed);

void drv_led_set_bridge_cb(drv_led_bridge_cb_t cb);

/**
 * @brief Initialize LED strips
 */
esp_err_t drv_led_init(void);

/**
 * @brief Set the LED system state for all lamps
 */
esp_err_t drv_led_set_state(drv_led_state_t state);

/**
 * @brief Set LED state from ISR context
 */
void drv_led_set_state_isr(drv_led_state_t state);

/**
 * @brief Control Functions for specific lamp_id (0 to 4)
 */
esp_err_t drv_led_set_base_color(uint8_t lamp_id, uint8_t r, uint8_t g, uint8_t b);
esp_err_t drv_led_set_color(uint8_t lamp_id, uint8_t r, uint8_t g, uint8_t b);
esp_err_t drv_led_set_effect(uint8_t lamp_id, drv_led_effect_t effect);
esp_err_t drv_led_set_brightness(uint8_t lamp_id, uint8_t brightness);
esp_err_t drv_led_set_speed(uint8_t lamp_id, uint8_t speed);
esp_err_t drv_led_set_power(uint8_t lamp_id, uint8_t power); // power = 1/0

esp_err_t drv_led_get_config(uint8_t lamp_id, uint8_t *power, uint8_t *r, uint8_t *g, uint8_t *b,
                             drv_led_effect_t *effect, uint8_t *brightness,
                             uint8_t *speed);

/**
 * @brief Begin a batch update - suppresses apply/bridge until commit.
 *        Use this to group multiple set_* calls into a single hardware update.
 */
void drv_led_begin_update(uint8_t lamp_id);

/**
 * @brief Commit a batch update - apply once + bridge once.
 *        Must be called after drv_led_begin_update() to flush changes.
 */
void drv_led_commit_update(uint8_t lamp_id);

// Save and Load settings
esp_err_t drv_led_save_settings(void);
esp_err_t drv_led_load_settings(void);
