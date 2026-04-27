if(CONFIG_APP_LCD_ENABLE)
    set(srcs "src/mgr_display.c" "src/mgr_display_ui.c")
    set(requires esp_lcd esp_lcd_gc9a01 lvgl drv_aht20 svc_wifi)
    set(priv_requires mgr_ac_logic mgr_fan_logic mgr_relay drv_led_strip)
else()
    set(srcs "")
    set(requires "")
    set(priv_requires "")
endif()

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "src"
                    REQUIRES ${requires}
                    PRIV_REQUIRES ${priv_requires})
