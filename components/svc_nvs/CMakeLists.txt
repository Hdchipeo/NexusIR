idf_component_register(SRCS "src/svc_nvs.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES svc_logging
                       REQUIRES json nvs_flash)
