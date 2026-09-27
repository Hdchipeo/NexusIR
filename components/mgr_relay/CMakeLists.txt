idf_component_register(SRCS "src/mgr_relay.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver button int_homekit svc_espnow svc_nvs nvs_flash)
