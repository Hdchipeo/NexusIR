#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_APP_LCD_ENABLE

/**
 * @brief Initialize the display manager (LCD + LVGL)
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mgr_display_init(void);

/**
 * @brief Update the temperature and humidity on the screen
 * 
 * @param temp Temperature in Celsius
 * @param hum Humidity in percentage
 */
void mgr_display_update_ui_sensor_safe(float temp, float hum);
void mgr_display_update_ui_weather_safe(const char *status);
void mgr_display_update_ui_weather_code_safe(int code);
void mgr_display_show_ui_notification_safe(const char *device_name, const char *status);
void mgr_display_update_ui_time_safe(void);
void mgr_display_activity_tick(void);
<<<<<<< HEAD
void mgr_display_wake(void);
bool mgr_display_is_dimmed(void);
=======
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d

#else

// Stubs for when LCD is disabled
static inline esp_err_t mgr_display_init(void) { return ESP_OK; }
static inline void mgr_display_update_ui_sensor_safe(float temp, float hum) {}
static inline void mgr_display_update_ui_weather_safe(const char *status) {}
static inline void mgr_display_update_ui_weather_code_safe(int code) {}
static inline void mgr_display_show_ui_notification_safe(const char *device_name, const char *status) {}
static inline void mgr_display_update_ui_time_safe(void) {}
static inline void mgr_display_activity_tick(void) {}
<<<<<<< HEAD
static inline void mgr_display_wake(void) {}
static inline bool mgr_display_is_dimmed(void) { return true; }
=======
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d

#endif // CONFIG_APP_LCD_ENABLE

#ifdef __cplusplus
}
#endif
