idf_component_register(SRCS "src/svc_ota.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES app_update esp_http_client esp_https_ota svc_logging svc_nvs drv_led_strip mbedtls nvs_flash svc_wifi)
