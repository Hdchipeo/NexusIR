idf_component_register(SRCS "src/svc_mdns.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES mdns svc_logging)
