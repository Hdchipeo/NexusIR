idf_component_register(SRCS "src/svc_wifi_prov.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_http_server svc_logging svc_nvs svc_web_server esp_netif esp_wifi nvs_flash
                       EMBED_FILES "www/index.html")
