idf_component_register(SRCS "src/int_rainmaker.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_rainmaker mgr_ac_logic svc_wifi svc_nvs json)
