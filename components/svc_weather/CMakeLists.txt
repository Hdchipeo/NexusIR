idf_component_register(SRCS "src/svc_weather.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_client json mgr_display)
