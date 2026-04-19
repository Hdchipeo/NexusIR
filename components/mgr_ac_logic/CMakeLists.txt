idf_component_register(SRCS "src/mgr_ac_logic.c"
                       INCLUDE_DIRS "include"
                       REQUIRES drv_ir_rmt
                       PRIV_REQUIRES svc_nvs svc_logging mgr_ir_protocols nvs_flash)
