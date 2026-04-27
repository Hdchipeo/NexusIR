#include "svc_weather.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mgr_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SVC_WEATHER";

// Open-Meteo Weather Codes Mapping
static const char* get_weather_desc(int code) {
    switch (code) {
        case 0: return "Clear sky";
        case 1:
        case 2:
        case 3: return "Cloudy";
        case 45:
        case 48: return "Foggy";
        case 51:
        case 53:
        case 55: return "Drizzle";
        case 61:
        case 63:
        case 65: return "Rainy";
        case 71:
        case 73:
        case 75: return "Snowy";
        case 80:
        case 81:
        case 82: return "Showers";
        case 95:
        case 96:
        case 99: return "Stormy";
        default: return "Sunny";
    }
}

static void weather_task(void *pvParameters) {
    // Wait for WiFi connection
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    char local_response_buffer[1024] = {0};
    esp_http_client_config_t config = {
        .url = "http://api.open-meteo.com/v1/forecast?latitude=21.0285&longitude=105.8542&current_weather=true",
        .user_data = local_response_buffer,
    };
    
    while (1) {
        ESP_LOGI(TAG, "Fetching weather data...");
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        
        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            int content_length = esp_http_client_get_content_length(client);
            ESP_LOGI(TAG, "HTTP Status = %d, content_length = %d", status_code, content_length);
            
            // Read response
            int len = esp_http_client_read_response(client, local_response_buffer, sizeof(local_response_buffer));
            if (len > 0) {
                local_response_buffer[len] = 0;
                cJSON *root = cJSON_Parse(local_response_buffer);
                if (root) {
                    cJSON *current = cJSON_GetObjectItem(root, "current_weather");
                    if (current) {
                        int weather_code = cJSON_GetObjectItem(current, "weathercode")->valueint;
                        const char *desc = get_weather_desc(weather_code);
                        ESP_LOGI(TAG, "Weather Code: %d, Desc: %s", weather_code, desc);
                        mgr_display_update_ui_weather_safe(desc);
                        mgr_display_update_ui_weather_code_safe(weather_code);
                    }
                    cJSON_Delete(root);
                }
            }
        } else {
            ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        }
        
        esp_http_client_cleanup(client);
        
        // Update every 30 minutes
        vTaskDelay(pdMS_TO_TICKS(30 * 60 * 1000));
    }
}

esp_err_t svc_weather_init(void) {
    xTaskCreate(weather_task, "weather_task", 8192, NULL, 5, NULL);
    return ESP_OK;
}
