idf_component_register(SRCS "src/mgr_ir_main.c" "src/mgr_ir_matrix.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES drv_ir_rmt svc_nvs svc_logging nvs_flash json drv_led_strip esp_driver_rmt esp_driver_gpio esp_timer)
