idf_component_register(SRCS "src/svc_web_server.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES svc_wifi svc_nvs mgr_ir_protocols mgr_ac_logic mgr_fan_logic svc_espnow json esp_http_server esp_driver_tsens spiffs esp_timer vfs int_rainmaker nvs_flash svc_logging svc_ota sys_utils drv_led_strip)
