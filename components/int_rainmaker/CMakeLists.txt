idf_component_register(SRCS "src/int_rainmaker.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_rainmaker mgr_ac_logic mgr_fan_logic mgr_relay drv_led_strip svc_wifi svc_nvs json mgr_ir_protocols svc_ota)
