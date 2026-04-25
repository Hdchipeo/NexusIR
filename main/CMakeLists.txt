idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES svc_wifi svc_nvs svc_web_server svc_ota svc_mdns svc_logging drv_led_strip drv_button mgr_ir_protocols mgr_ac_logic mgr_fan_logic drv_aht20 int_rainmaker int_homekit sys_utils svc_espnow esp_driver_ledc mgr_relay)
