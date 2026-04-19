idf_component_register(SRCS "src/svc_wifi.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi
                       PRIV_REQUIRES nvs_flash svc_logging svc_nvs svc_wifi_prov svc_mdns int_homekit drv_led_strip espressif__network_provisioning)
