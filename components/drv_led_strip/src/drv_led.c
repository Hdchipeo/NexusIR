#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "driver/gpio.h"
#include "nvs.h"
#include "drv_led.h"
#include "driver/ledc.h"

#define TAG "drv_led"

typedef struct {
    bool enabled;
    bool is_rgb_strip;
    int gpio;
    int max_leds;
    led_strip_handle_t strip;
    uint8_t r, g, b;
    drv_led_effect_t effect;
    uint8_t brightness;
    uint8_t speed;
    uint8_t power;
} led_context_t;

static led_context_t s_leds[MAX_LEDS] = {0};
static drv_led_state_t g_led_state = DRV_LED_OFF;
static drv_led_bridge_cb_t s_bridge_cb = NULL;

void drv_led_set_bridge_cb(drv_led_bridge_cb_t cb) {
  s_bridge_cb = cb;
}

static void drv_led_trigger_bridge_update(uint8_t lamp_id) {
  if (!s_bridge_cb || lamp_id >= MAX_LEDS) return;
  s_bridge_cb(lamp_id, s_leds[lamp_id].power, (uint8_t)s_leds[lamp_id].effect, 
              s_leds[lamp_id].brightness, s_leds[lamp_id].r, s_leds[lamp_id].g, 
              s_leds[lamp_id].b, s_leds[lamp_id].speed);
}

static void led_strip_set_pixel_scaled(uint8_t id, int index, uint8_t r, uint8_t g, uint8_t b) {
  if (!s_leds[id].enabled || index >= s_leds[id].max_leds) return;
  float scale = (float)s_leds[id].brightness / 100.0f;
  if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
    led_strip_set_pixel(s_leds[id].strip, index, (uint8_t)(r * scale), (uint8_t)(g * scale), (uint8_t)(b * scale));
  }
}

static void led_refresh_logic(uint8_t id) {
  if (!s_leds[id].enabled) return;
  if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
    led_strip_refresh(s_leds[id].strip);
  } else if (!s_leds[id].is_rgb_strip) {
    uint32_t duty = 0;
    if (s_leds[id].power && g_led_state != DRV_LED_OFF) {
      uint8_t r = s_leds[id].r, g = s_leds[id].g, b = s_leds[id].b;
      uint32_t val = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
      duty = (val * s_leds[id].brightness * 8191) / (255 * 100);
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(LEDC_CHANNEL_0 + id), duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(LEDC_CHANNEL_0 + id));
  }
}

static void run_static_effect(uint8_t id) {
  for (int i = 0; i < s_leds[id].max_leds; i++) {
    led_strip_set_pixel_scaled(id, i, s_leds[id].r, s_leds[id].g, s_leds[id].b);
  }
  led_refresh_logic(id);
}

static void led_task_entry(void *arg) {
  while (1) {
    for (int id = 0; id < MAX_LEDS; id++) {
      if (!s_leds[id].enabled) continue;
      
      if (g_led_state == DRV_LED_OFF || !s_leds[id].power) {
        if (s_leds[id].is_rgb_strip && s_leds[id].strip) led_strip_clear(s_leds[id].strip);
        led_refresh_logic(id);
      } else {
        // Simplified for now: just static color based on global state or individual color
        if (g_led_state == DRV_LED_IDLE) {
          run_static_effect(id);
        } else {
          // System state override
          uint8_t r=0, g=20, b=0; // Default green for system states
          if (g_led_state == DRV_LED_IR_TX || g_led_state == DRV_LED_IR_FAIL) { r=20; g=0; }
          else if (g_led_state == DRV_LED_WIFI_PROV) { r=0; g=0; b=20; }
          
          for (int i = 0; i < s_leds[id].max_leds; i++) led_strip_set_pixel_scaled(id, i, r, g, b);
          led_refresh_logic(id);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

esp_err_t drv_led_init(void) {
#ifdef CONFIG_APP_LED1_CONTROL
  s_leds[0].enabled = true;
  s_leds[0].gpio = CONFIG_APP_LED1_GPIO;
  #ifdef CONFIG_APP_LED1_TYPE_RGB_STRIP
  s_leds[0].is_rgb_strip = true;
  s_leds[0].max_leds = CONFIG_APP_LED1_COUNT;
  #endif
#endif
#ifdef CONFIG_APP_LED2_CONTROL
  s_leds[1].enabled = true;
  s_leds[1].gpio = CONFIG_APP_LED2_GPIO;
  #ifdef CONFIG_APP_LED2_TYPE_RGB_STRIP
  s_leds[1].is_rgb_strip = true;
  s_leds[1].max_leds = CONFIG_APP_LED2_COUNT;
  #endif
#endif
#ifdef CONFIG_APP_LED3_CONTROL
  s_leds[2].enabled = true;
  s_leds[2].gpio = CONFIG_APP_LED3_GPIO;
  #ifdef CONFIG_APP_LED3_TYPE_RGB_STRIP
  s_leds[2].is_rgb_strip = true;
  s_leds[2].max_leds = CONFIG_APP_LED3_COUNT;
  #endif
#endif
#ifdef CONFIG_APP_LED4_CONTROL
  s_leds[3].enabled = true;
  s_leds[3].gpio = CONFIG_APP_LED4_GPIO;
  #ifdef CONFIG_APP_LED4_TYPE_RGB_STRIP
  s_leds[3].is_rgb_strip = true;
  s_leds[3].max_leds = CONFIG_APP_LED4_COUNT;
  #endif
#endif
#ifdef CONFIG_APP_LED5_CONTROL
  s_leds[4].enabled = true;
  s_leds[4].gpio = CONFIG_APP_LED5_GPIO;
  #ifdef CONFIG_APP_LED5_TYPE_RGB_STRIP
  s_leds[4].is_rgb_strip = true;
  s_leds[4].max_leds = CONFIG_APP_LED5_COUNT;
  #endif
#endif

  drv_led_load_settings();

  for (int id = 0; id < MAX_LEDS; id++) {
    if (!s_leds[id].enabled) continue;
    
    if (s_leds[id].is_rgb_strip) {
      led_strip_config_t strip_config = { .strip_gpio_num = s_leds[id].gpio, .max_leds = s_leds[id].max_leds };
      led_strip_spi_config_t spi_config = { .spi_bus = (spi_host_device_t)SPI2_HOST, .flags.with_dma = true };
      led_strip_new_spi_device(&strip_config, &spi_config, &s_leds[id].strip);
      led_strip_clear(s_leds[id].strip);
    } else {
      ledc_timer_config_t ledc_timer = {
          .speed_mode       = LEDC_LOW_SPEED_MODE,
          .timer_num        = LEDC_TIMER_0,
          .duty_resolution  = LEDC_TIMER_13_BIT,
          .freq_hz          = 5000,
          .clk_cfg          = LEDC_AUTO_CLK
      };
      ledc_timer_config(&ledc_timer);

      ledc_channel_config_t ledc_channel = {
          .speed_mode     = LEDC_LOW_SPEED_MODE,
          .channel        = (ledc_channel_t)(LEDC_CHANNEL_0 + id),
          .timer_sel      = LEDC_TIMER_0,
          .intr_type      = LEDC_INTR_DISABLE,
          .gpio_num       = s_leds[id].gpio,
          .duty           = 0,
          .hpoint         = 0
      };
      ledc_channel_config(&ledc_channel);
    }
  }

  xTaskCreate(led_task_entry, "led_task", 4096, NULL, 5, NULL);
  return ESP_OK;
}

typedef struct {
    uint8_t r, g, b;
    uint8_t brightness;
    uint8_t speed;
    uint8_t effect;
    uint8_t power;
} led_save_ctx_t;

esp_err_t drv_led_save_settings(void) {
  nvs_handle_t h; 
  if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
    led_save_ctx_t saves[MAX_LEDS];
    for(int i=0; i<MAX_LEDS; i++) {
        saves[i].r = s_leds[i].r; saves[i].g = s_leds[i].g; saves[i].b = s_leds[i].b;
        saves[i].brightness = s_leds[i].brightness; saves[i].speed = s_leds[i].speed;
        saves[i].effect = s_leds[i].effect; saves[i].power = s_leds[i].power;
    }
    nvs_set_blob(h, "leds_ctx_v3", saves, sizeof(saves));
    nvs_commit(h);
    nvs_close(h);
  }
  return ESP_OK;
}

esp_err_t drv_led_load_settings(void) {
  // Setup defaults first
  for (int id = 0; id < MAX_LEDS; id++) {
    s_leds[id].power = 0;
    s_leds[id].r = 255; s_leds[id].g = 255; s_leds[id].b = 255;
    s_leds[id].brightness = 100;
    s_leds[id].speed = 50;
    s_leds[id].effect = DRV_LED_EFFECT_STATIC;
  }

  nvs_handle_t h; 
  if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
    led_save_ctx_t saves[MAX_LEDS];
    size_t size = sizeof(saves);
    if (nvs_get_blob(h, "leds_ctx_v3", saves, &size) == ESP_OK) {
        for(int i=0; i<MAX_LEDS; i++) {
            s_leds[i].r = saves[i].r; s_leds[i].g = saves[i].g; s_leds[i].b = saves[i].b;
            s_leds[i].brightness = saves[i].brightness; s_leds[i].speed = saves[i].speed;
            s_leds[i].effect = (drv_led_effect_t)saves[i].effect; s_leds[i].power = saves[i].power;
        }
    }
    nvs_close(h);
  }
  return ESP_OK;
}

esp_err_t drv_led_set_state(drv_led_state_t state) { g_led_state = state; return ESP_OK; }
void drv_led_set_state_isr(drv_led_state_t state) { g_led_state = state; }

esp_err_t drv_led_set_base_color(uint8_t id, uint8_t r, uint8_t g, uint8_t b) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].r = r; s_leds[id].g = g; s_leds[id].b = b;
  drv_led_trigger_bridge_update(id);
  return ESP_OK;
}
esp_err_t drv_led_set_color(uint8_t id, uint8_t r, uint8_t g, uint8_t b) { return drv_led_set_base_color(id, r, g, b); }
esp_err_t drv_led_set_effect(uint8_t id, drv_led_effect_t effect) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].effect = effect;
  drv_led_trigger_bridge_update(id);
  return ESP_OK;
}
esp_err_t drv_led_set_brightness(uint8_t id, uint8_t brightness) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].brightness = brightness > 100 ? 100 : brightness;
  drv_led_trigger_bridge_update(id);
  return ESP_OK;
}
esp_err_t drv_led_set_speed(uint8_t id, uint8_t speed) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].speed = speed;
  drv_led_trigger_bridge_update(id);
  return ESP_OK;
}
esp_err_t drv_led_set_power(uint8_t id, uint8_t power) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].power = power;
  drv_led_trigger_bridge_update(id);
  return ESP_OK;
}
esp_err_t drv_led_get_config(uint8_t id, uint8_t *power, uint8_t *r, uint8_t *g, uint8_t *b,
                             drv_led_effect_t *effect, uint8_t *brightness, uint8_t *speed) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  if (power) *power = s_leds[id].power;
  if (r) *r = s_leds[id].r;
  if (g) *g = s_leds[id].g;
  if (b) *b = s_leds[id].b;
  if (effect) *effect = s_leds[id].effect;
  if (brightness) *brightness = s_leds[id].brightness;
  if (speed) *speed = s_leds[id].speed;
  return ESP_OK;
}
