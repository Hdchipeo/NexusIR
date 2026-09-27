idf_component_register(SRCS "src/mgr_fan_logic.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash mgr_ir_protocols drv_ir_rmt)
