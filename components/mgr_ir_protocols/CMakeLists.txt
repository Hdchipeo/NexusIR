idf_component_register(SRCS "src/mgr_ir_impl.cpp" "src/ir_ac_registry.cpp" "src/ir_universal.cpp" "src/mgr_ir_main.c" "src/ir_protocol_stubs.cpp"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES drv_ir_rmt svc_nvs svc_logging nvs_flash json drv_led_strip esp_driver_rmt esp_driver_gpio esp_timer)
