idf_component_register(SRCS "src/drv_led.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES led_strip svc_nvs svc_logging nvs_flash esp_driver_gpio esp_driver_ledc
)
