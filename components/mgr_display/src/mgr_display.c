#include "mgr_display.h"

#if CONFIG_APP_LCD_ENABLE

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
<<<<<<< HEAD
#include "freertos/timers.h"
#include "driver/ledc.h"
=======
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
#include "lvgl.h"

static SemaphoreHandle_t lv_mux = NULL;

static const char *TAG = "MGR_DISPLAY";

// Pin definitions from Kconfig
#define LCD_GPIO_LED CONFIG_APP_LCD_BACKLIGHT_GPIO
#define LCD_GPIO_CS CONFIG_APP_LCD_CS_GPIO
#define LCD_GPIO_DC CONFIG_APP_LCD_DC_GPIO
#define LCD_GPIO_RST CONFIG_APP_LCD_RST_GPIO
#define LCD_GPIO_SCLK CONFIG_APP_LCD_SCLK_GPIO
#define LCD_GPIO_MOSI CONFIG_APP_LCD_MOSI_GPIO

#define LCD_H_RES CONFIG_APP_LCD_H_RES
#define LCD_V_RES CONFIG_APP_LCD_V_RES

#define LCD_SPI_HOST (CONFIG_APP_LCD_SPI_HOST == 0 ? SPI2_HOST : SPI3_HOST)

static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_drv_t disp_drv; // Static to be used in callback context

<<<<<<< HEAD
static TimerHandle_t wake_timer = NULL;
static bool s_is_dimmed = false;

#define BRIGHTNESS_ACTIVE 255 // 100%
#define BRIGHTNESS_IDLE   25  // ~10% (out of 255)

static void mgr_display_set_brightness(uint8_t value) {
  esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, value);
  if (err == ESP_OK) {
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
  } else {
      ESP_LOGE(TAG, "Failed to set LEDC duty: %s", esp_err_to_name(err));
  }
  s_is_dimmed = (value < BRIGHTNESS_ACTIVE);
}

static void wake_timer_cb(TimerHandle_t xTimer) {
  ESP_LOGI(TAG, "Display timeout, dimming to idle level");
  mgr_display_set_brightness(BRIGHTNESS_IDLE);
}

=======
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
// Function declarations from mgr_display_ui.c
void mgr_display_ui_init(void);
void mgr_display_update_ui_time(void);
void mgr_display_update_ui_sensor(float temp, float hum);
void mgr_display_show_ui_notification(const char *device_name,
                                      const char *status);

void mgr_display_activity_tick(void) {
<<<<<<< HEAD
  mgr_display_wake();
=======
  gpio_set_level(LCD_GPIO_LED, 1); // Always ON
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                          lv_color_t *color_map) {
  esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1,
                            area->y2 + 1, color_map);
  lv_disp_flush_ready(drv);
}

static void lv_tick_task(void *arg) {
  while (1) {
    lv_tick_inc(10);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static void display_task(void *arg) {
  ESP_LOGI(TAG, "Starting LVGL task loop...");
  while (1) {
    uint32_t task_delay_ms = 10;
    if (xSemaphoreTakeRecursive(lv_mux, pdMS_TO_TICKS(1000)) == pdTRUE) {
      task_delay_ms = lv_timer_handler();
      xSemaphoreGiveRecursive(lv_mux);
    } else {
      ESP_LOGE(TAG, "CRITICAL: Display task blocked for 1000ms!");
    }
<<<<<<< HEAD
=======
    // Keep backlight always ON
    gpio_set_level(LCD_GPIO_LED, 1);
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d

    // Cap delay to prevent display_task sleeping forever, but allow enough for
    // power saving
    if (task_delay_ms < 10)
      task_delay_ms = 10;
    if (task_delay_ms > 500)
      task_delay_ms = 500;
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

esp_err_t mgr_display_init(void) {
  ESP_LOGI(TAG, "Initializing LCD GC9A01...");

  lv_mux = xSemaphoreCreateRecursiveMutex();
  if (lv_mux == NULL) {
    return ESP_ERR_NO_MEM;
  }

<<<<<<< HEAD
  // 1. Initialize Backlight with LEDC PWM
  ESP_LOGI(TAG, "Initializing Backlight PWM on GPIO %d (Channel 5, Timer 1)", LCD_GPIO_LED);
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_8_BIT,
      .timer_num = LEDC_TIMER_1,
      .freq_hz = 1000, // Reduced to 1kHz
      .clk_cfg = LEDC_AUTO_CLK};
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                        .channel = LEDC_CHANNEL_5,
                                        .timer_sel = LEDC_TIMER_1,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .gpio_num = LCD_GPIO_LED,
                                        .duty = BRIGHTNESS_ACTIVE, // Start at 100%
                                        .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  // 1.1 Initialize Wake Timer
  wake_timer = xTimerCreate("display_wake", pdMS_TO_TICKS(30000), pdFALSE, NULL,
                            wake_timer_cb);
  if (wake_timer == NULL) {
    ESP_LOGE(TAG, "Failed to create wake timer");
    return ESP_ERR_NO_MEM;
  }
  xTimerStart(wake_timer, 0);
=======
  // 1. Initialize Backlight
  gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT,
                                  .pin_bit_mask = 1ULL << LCD_GPIO_LED};
  gpio_config(&bk_gpio_config);
  gpio_set_level(LCD_GPIO_LED, 1); // Turn on backlight
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d

  // 2. Initialize SPI Bus
  spi_bus_config_t buscfg = {
      .sclk_io_num = LCD_GPIO_SCLK,
      .mosi_io_num = LCD_GPIO_MOSI,
      .miso_io_num = -1,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
  };
  ESP_LOGI(TAG, "Initializing SPI Bus...");
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

  // 3. Initialize Panel IO
  ESP_LOGI(TAG, "Configuring LCD IO...");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .dc_gpio_num = LCD_GPIO_DC,
      .cs_gpio_num = LCD_GPIO_CS,
      .pclk_hz = 40 * 1000 * 1000, // Reduced to 40MHz for debug stability
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .spi_mode = 0,
      .trans_queue_depth = 10,
      .on_color_trans_done =
          NULL, // flush_ready is called directly in lvgl_flush_cb
      .user_ctx = NULL,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST,
                                           &io_config, &io_handle));

  // 4. Initialize Panel Driver
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = LCD_GPIO_RST,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_LOGI(TAG, "Creating LCD Panel...");
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
  ESP_LOGI(TAG, "Resetting and Initializing Panel...");
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_LOGI(TAG, "Setting Inversion, Mirror, and Rotation...");
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  // 5. Initialize LVGL
  ESP_LOGI(TAG, "Initializing LVGL Library...");
  lv_init();
  ESP_LOGI(TAG, "Allocating Display Buffers...");
  lv_color_t *buf1 =
      heap_caps_malloc(LCD_H_RES * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
  lv_color_t *buf2 =
      heap_caps_malloc(LCD_H_RES * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
  if (!buf1 || !buf2) {
    ESP_LOGE(TAG, "Failed to allocate display buffers!");
    return ESP_ERR_NO_MEM;
  }
  static lv_disp_draw_buf_t draw_buf;
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_H_RES * 40);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.user_data = panel_handle;
  lv_disp_drv_register(&disp_drv);

  // 6. Initialize UI
  ESP_LOGI(TAG, "Initializing UI Components...");
  mgr_display_ui_init();

  // 7. Start Tasks
  ESP_LOGI(TAG, "Starting LVGL Tasks...");
  xTaskCreate(lv_tick_task, "lvgl_tick", 2048, NULL, 5, NULL);
  xTaskCreate(display_task, "display_task", 8192, NULL, 3, NULL);

  mgr_display_activity_tick(); // Initialize activity timer

  return ESP_OK;
}

bool mgr_display_lock(int timeout_ms) {
  if (lv_mux == NULL)
    return false;
  bool ret =
      xSemaphoreTakeRecursive(lv_mux, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
  if (!ret) {
    ESP_LOGE(TAG, "UI Lock FAILED after %dms!", timeout_ms);
  }
  return ret;
}

void mgr_display_unlock(void) {
  if (lv_mux != NULL) {
    xSemaphoreGiveRecursive(lv_mux);
  }
}

void mgr_display_update_ui_weather(const char *status);
void mgr_display_update_ui_weather_code(int code);

void mgr_display_update_ui_weather_safe(const char *status) {
  if (mgr_display_lock(1000)) {
    mgr_display_update_ui_weather(status);
    mgr_display_unlock();
  }
}

void mgr_display_update_ui_weather_code_safe(int code) {
  if (mgr_display_lock(1000)) {
    mgr_display_update_ui_weather_code(code);
    mgr_display_unlock();
    mgr_display_activity_tick();
  }
}

void mgr_display_update_ui_sensor_safe(float temp, float hum) {
  if (mgr_display_lock(1000)) {
    ESP_LOGI(TAG, "UI Sensor Update: T=%.1f H=%.0f", temp, hum);
    mgr_display_update_ui_sensor(temp, hum);
    mgr_display_unlock();
  } else {
    ESP_LOGW(TAG, "UI Sensor Update SKIPPED: lock timeout");
  }
}

void mgr_display_show_ui_notification_safe(const char *device_name,
                                           const char *status) {
  if (mgr_display_lock(1000)) {
    ESP_LOGI(TAG, "UI Notify: [%s] -> %s", device_name, status);
    mgr_display_show_ui_notification(device_name, status);
    mgr_display_unlock();
    mgr_display_activity_tick();
  } else {
    ESP_LOGW(TAG, "UI Notify SKIPPED: lock timeout");
  }
}

void mgr_display_update_ui_time_safe(void) {
  if (mgr_display_lock(1000)) {
    mgr_display_update_ui_time();
    mgr_display_unlock();
  } else {
    ESP_LOGW(TAG, "UI Time Update SKIPPED: lock timeout");
  }
}
<<<<<<< HEAD

void mgr_display_wake(void) {
  if (wake_timer == NULL)
    return;
  ESP_LOGI(TAG, "Wake event: Setting brightness to 100%%");
  mgr_display_set_brightness(BRIGHTNESS_ACTIVE);
  xTimerReset(wake_timer, 0);
}

bool mgr_display_is_dimmed(void) { return s_is_dimmed; }
=======
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
#endif // CONFIG_APP_LCD_ENABLE
