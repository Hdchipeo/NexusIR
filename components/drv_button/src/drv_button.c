#include "drv_button.h"
#include "driver/gpio.h"
#include "drv_led.h"
#include "esp_log.h"
#include "esp_system.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "svc_log.h"
#include "mgr_display.h"

#define TAG "drv_button"
#define BUTTON_GPIO CONFIG_APP_BUTTON_GPIO

static void button_single_click_cb(void *arg, void *data) {
  ESP_LOGI(TAG, "Button Tap - Wake up display");
  mgr_display_activity_tick();
}

static void button_long_press_cb(void *arg, void *data) {
  ESP_LOGI(TAG, "Button Long Pressed (3s) - Factory Reset");
  drv_led_set_state(DRV_LED_IR_LEARN); // Visual indication
  nvs_flash_erase();
  esp_restart();
}

esp_err_t drv_button_init(void) {
  button_config_t btn_cfg = {
      .long_press_time = 3000,
      .short_press_time = 50,
  };
  button_gpio_config_t gpio_cfg = {
      .gpio_num = BUTTON_GPIO,
      .active_level = CONFIG_APP_BUTTON_ACTIVE_LEVEL,
  };

  button_handle_t btn_handle = NULL;
  iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle);

  if (btn_handle) {
    iot_button_register_cb(btn_handle, BUTTON_PRESS_DOWN, NULL,
                           button_single_click_cb, NULL); // Wake up on ANY press
    iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, NULL,
                           button_long_press_cb, NULL);
    ESP_LOGI(TAG, "Button initialized on GPIO %d", BUTTON_GPIO);
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "Button create failed");
    return ESP_FAIL;
  }
}
