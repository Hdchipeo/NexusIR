#ifndef SVC_WEATHER_H
#define SVC_WEATHER_H

#include "esp_err.h"

/**
 * @brief Initialize the weather service
 * @return esp_err_t 
 */
esp_err_t svc_weather_init(void);

#endif // SVC_WEATHER_H
