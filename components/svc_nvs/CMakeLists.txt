idf_component_register(SRCS "src/svc_nvs.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES nvs_flash svc_logging
                       REQUIRES json)
