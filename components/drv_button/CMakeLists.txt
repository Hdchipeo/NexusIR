idf_component_register(SRCS "src/drv_button.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES driver button log esp_system nvs_flash svc_logging drv_led_strip mgr_display)
