idf_component_register(SRCS "src/int_homekit.c"
                       INCLUDE_DIRS "include"
                       REQUIRES mgr_ac_logic mgr_fan_logic
<<<<<<< HEAD
                       PRIV_REQUIRES esp_hap_core svc_wifi svc_nvs json esp_hap_apple_profiles app_hap_setup_payload svc_web_server drv_aht20 drv_led_strip mgr_relay mgr_ir_protocols)
=======
                       PRIV_REQUIRES esp_hap_core svc_wifi svc_nvs json esp_hap_apple_profiles app_hap_setup_payload svc_web_server drv_aht20 drv_led_strip mgr_relay)
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
