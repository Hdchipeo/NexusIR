idf_component_register(SRCS "src/mgr_display.c" "src/mgr_display_ui.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "src"
                    REQUIRES esp_lcd esp_lcd_gc9a01 lvgl drv_aht20 svc_wifi esp_driver_gpio esp_timer driver
                    PRIV_REQUIRES mgr_ac_logic mgr_fan_logic mgr_relay drv_led_strip)
