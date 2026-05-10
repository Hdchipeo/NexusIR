idf_component_register(
  SRCS "src/svc_espnow.c"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES "esp_wifi" "drv_led_strip" "drv_ir_rmt" "mgr_ir_protocols" "nvs_flash" "mgr_display"
)
