idf_component_register(SRCS "src/drv_aht20.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES button i2c_bus espressif__aht20 driver esp_timer)
