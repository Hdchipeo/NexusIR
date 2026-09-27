#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "driver/rmt_types.h"
#include "driver/gpio.h"
#include "nvs.h"
#include "drv_led.h"
#include "driver/ledc.h"

#define TAG "drv_led"
#ifndef CONFIG_APP_LED_STRIP_MAX_OUTPUT
#define CONFIG_APP_LED_STRIP_MAX_OUTPUT 80
#endif

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
    bool dirty;
    bool batch_mode;  // When true, set_* skips apply+bridge
} led_context_t;

static led_context_t s_leds[MAX_LEDS] = {0};
static drv_led_state_t g_led_state = DRV_LED_OFF;
static drv_led_bridge_cb_t s_bridge_cb = NULL;
static SemaphoreHandle_t s_led_lock = NULL;

static void led_lock(void) {
  if (s_led_lock) xSemaphoreTake(s_led_lock, portMAX_DELAY);
}

static void led_unlock(void) {
  if (s_led_lock) xSemaphoreGive(s_led_lock);
}

static led_color_component_format_t led_get_color_format(void) {
#if CONFIG_APP_LED_STRIP_COLOR_ORDER_RGB
  return LED_STRIP_COLOR_COMPONENT_FMT_RGB;
#elif CONFIG_APP_LED_STRIP_COLOR_ORDER_BRG
  return (led_color_component_format_t){.format = {.r_pos = 1, .g_pos = 2, .b_pos = 0, .w_pos = 3, .bytes_per_color = 1, .num_components = 3}};
#elif CONFIG_APP_LED_STRIP_COLOR_ORDER_RBG
  return (led_color_component_format_t){.format = {.r_pos = 0, .g_pos = 2, .b_pos = 1, .w_pos = 3, .bytes_per_color = 1, .num_components = 3}};
#elif CONFIG_APP_LED_STRIP_COLOR_ORDER_BGR
  return (led_color_component_format_t){.format = {.r_pos = 2, .g_pos = 1, .b_pos = 0, .w_pos = 3, .bytes_per_color = 1, .num_components = 3}};
#elif CONFIG_APP_LED_STRIP_COLOR_ORDER_GBR
  return (led_color_component_format_t){.format = {.r_pos = 2, .g_pos = 0, .b_pos = 1, .w_pos = 3, .bytes_per_color = 1, .num_components = 3}};
#else
  return LED_STRIP_COLOR_COMPONENT_FMT_GRB;
#endif
}

void drv_led_set_bridge_cb(drv_led_bridge_cb_t cb) {
  s_bridge_cb = cb;
}

static void drv_led_trigger_bridge_update(uint8_t lamp_id) {
  if (!s_bridge_cb || lamp_id >= MAX_LEDS) return;
  s_bridge_cb(lamp_id, s_leds[lamp_id].power, (uint8_t)s_leds[lamp_id].effect, 
              s_leds[lamp_id].brightness, s_leds[lamp_id].r, s_leds[lamp_id].g, 
              s_leds[lamp_id].b, s_leds[lamp_id].speed);
}

static void led_set_pixel_system(uint8_t id, int index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  if (!s_leds[id].enabled || index >= s_leds[id].max_leds) return;
  if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
    uint16_t max_output = CONFIG_APP_LED_STRIP_MAX_OUTPUT > 255 ? 255 : CONFIG_APP_LED_STRIP_MAX_OUTPUT;
    uint8_t sr = (uint8_t)(((uint32_t)r * brightness * max_output) / (100 * 255));
    uint8_t sg = (uint8_t)(((uint32_t)g * brightness * max_output) / (100 * 255));
    uint8_t sb = (uint8_t)(((uint32_t)b * brightness * max_output) / (100 * 255));
    led_strip_set_pixel(s_leds[id].strip, index, sr, sg, sb);
  }
}

static void led_refresh_system(uint8_t id, uint8_t mono_brightness) {
  if (!s_leds[id].enabled) return;
  if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
    led_strip_refresh(s_leds[id].strip);
  } else if (!s_leds[id].is_rgb_strip) {
    uint32_t duty = (mono_brightness * 8191) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(LEDC_CHANNEL_0 + id), duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(LEDC_CHANNEL_0 + id));
  }
}

static void led_strip_set_pixel_scaled(uint8_t id, int index, uint8_t r, uint8_t g, uint8_t b) {
  if (!s_leds[id].enabled || index >= s_leds[id].max_leds) return;
  uint16_t brightness = s_leds[id].brightness > 100 ? 100 : s_leds[id].brightness;
  uint16_t max_output = CONFIG_APP_LED_STRIP_MAX_OUTPUT > 255 ? 255 : CONFIG_APP_LED_STRIP_MAX_OUTPUT;
  if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
    uint8_t sr = (uint8_t)(((uint32_t)r * brightness * max_output) / (100 * 255));
    uint8_t sg = (uint8_t)(((uint32_t)g * brightness * max_output) / (100 * 255));
    uint8_t sb = (uint8_t)(((uint32_t)b * brightness * max_output) / (100 * 255));
    led_strip_set_pixel(s_leds[id].strip, index, sr, sg, sb);
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
  s_leds[id].dirty = false;
}

static void drv_led_apply(uint8_t id) {
  if (id >= MAX_LEDS || !s_leds[id].enabled) return;

  led_lock();
  if (g_led_state == DRV_LED_OFF) {
    if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
      led_strip_clear(s_leds[id].strip);
    }
    led_refresh_logic(id);
    s_leds[id].dirty = false;
    led_unlock();
    return;
  }

  if (g_led_state == DRV_LED_IDLE) {
    if (!s_leds[id].power) {
      if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
        led_strip_clear(s_leds[id].strip);
      }
      led_refresh_logic(id);
    } else {
      run_static_effect(id);
    }
    s_leds[id].dirty = false;
  }
  led_unlock();
}

static void led_task_entry(void *arg) {
  static drv_led_state_t s_last_state = DRV_LED_OFF;
  static uint32_t s_state_frame_count = 0;

  while (1) {
    led_lock();
    if (g_led_state != s_last_state) {
      s_last_state = g_led_state;
      s_state_frame_count = 0;
    }
    led_unlock();

    for (int id = 0; id < MAX_LEDS; id++) {
      if (!s_leds[id].enabled) continue;

      led_lock();
      if (g_led_state == DRV_LED_OFF) {
        if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
          led_strip_clear(s_leds[id].strip);
        }
        led_refresh_system(id, 0);
        s_leds[id].dirty = false;
        led_unlock();
        continue;
      }

      if (g_led_state == DRV_LED_IDLE) {
        if (!s_leds[id].dirty) {
          led_unlock();
          continue;
        }
        if (!s_leds[id].power) {
          if (s_leds[id].is_rgb_strip && s_leds[id].strip) {
            led_strip_clear(s_leds[id].strip);
          }
          led_refresh_system(id, 0);
        } else {
          run_static_effect(id);
        }
        s_leds[id].dirty = false;
        led_unlock();
        continue;
      }

      // System state override - frame-by-frame animation rendering
      uint32_t f = s_state_frame_count;
      uint8_t mono_br = 0;

      switch (g_led_state) {
        case DRV_LED_STARTUP: {
          // Orange breathing (cycle = 2s = 20 frames)
          int cycle = f % 20;
          int step = (cycle < 10) ? cycle : (20 - cycle);
          uint8_t br = 10 + step * 8; // 10% to 90%
          mono_br = br;
          for (int i = 0; i < s_leds[id].max_leds; i++) {
            led_set_pixel_system(id, i, 255, 100, 0, br);
          }
          break;
        }
        case DRV_LED_WIFI_PROV: {
          // Blue chase (WS2812) or Double Blink (Mono)
          if (s_leds[id].is_rgb_strip) {
            int active = f % s_leds[id].max_leds;
            for (int i = 0; i < s_leds[id].max_leds; i++) {
              if (i == active) {
                led_set_pixel_system(id, i, 0, 0, 255, 100);
              } else if (i == (active + s_leds[id].max_leds - 1) % s_leds[id].max_leds) {
                led_set_pixel_system(id, i, 0, 0, 255, 40);
              } else {
                led_set_pixel_system(id, i, 0, 0, 255, 10);
              }
            }
          } else {
            int cycle = f % 12; // 1.2s cycle
            mono_br = (cycle == 0 || cycle == 1 || cycle == 3 || cycle == 4) ? 80 : 0;
          }
          break;
        }
        case DRV_LED_WIFI_CONN: {
          // Cyan blinking (600ms cycle: 3 frames ON, 3 frames OFF)
          bool on = (f % 6) < 3;
          mono_br = on ? 80 : 0;
          for (int i = 0; i < s_leds[id].max_leds; i++) {
            if (on) {
              led_set_pixel_system(id, i, 0, 150, 150, 100);
            } else {
              led_set_pixel_system(id, i, 0, 0, 0, 0);
            }
          }
          break;
        }
        case DRV_LED_WIFI_SUCCESS: {
          // Green solid
          mono_br = 80;
          for (int i = 0; i < s_leds[id].max_leds; i++) {
            led_set_pixel_system(id, i, 0, 255, 0, 100);
          }
          break;
        }
        case DRV_LED_WIFI_FAIL: {
          // Fast Red blinking (200ms cycle)
          bool on = (f % 2) == 0;
          mono_br = on ? 100 : 0;
          for (int i = 0; i < s_leds[id].max_leds; i++) {
            if (on) {
              led_set_pixel_system(id, i, 255, 0, 0, 100);
            } else {
              led_set_pixel_system(id, i, 0, 0, 0, 0);
            }
          }
          break;
        }
        case DRV_LED_OTA: {
          // Purple chase (WS2812) or Rapid blink (Mono)
          if (s_leds[id].is_rgb_strip) {
            int active = f % s_leds[id].max_leds;
            for (int i = 0; i < s_leds[id].max_leds; i++) {
              if (i == active) {
                led_set_pixel_system(id, i, 180, 0, 180, 100);
              } else if (i == (active + s_leds[id].max_leds - 1) % s_leds[id].max_leds) {
                led_set_pixel_system(id, i, 180, 0, 180, 40);
              } else {
                led_set_pixel_system(id, i, 0, 0, 0, 0);
              }
            }
          } else {
            mono_br = (f % 2 == 0) ? 100 : 0;
          }
          break;
        }
        case DRV_LED_IR_TX: {
          // Solid white
          mono_br = 100;
          for (int i = 0; i < s_leds[id].max_leds; i++) {
            led_set_pixel_system(id, i, 255, 255, 255, 100);
          }
          break;
        }
        case DRV_LED_IR_LEARN: {
          // Yellow breathing (1.6s cycle)
          int cycle = f % 16;
          int step = (cycle < 8) ? cycle : (16 - cycle);
          uint8_t br = 10 + step * 10;
          mono_br = br;
          for (int i = 0; i < s_leds[id].max_leds; i++) {
            led_set_pixel_system(id, i, 255, 180, 0, br);
          }
          break;
        }
        case DRV_LED_IR_SUCCESS: {
          // Solid Green
          mono_br = 80;
          for (int i = 0; i < s_leds[id].max_leds; i++) {
            led_set_pixel_system(id, i, 0, 255, 0, 100);
          }
          break;
        }
        case DRV_LED_IR_FAIL: {
          // Solid Red
          mono_br = 80;
          for (int i = 0; i < s_leds[id].max_leds; i++) {
            led_set_pixel_system(id, i, 255, 0, 0, 100);
          }
          break;
        }
        default:
          break;
      }

      led_refresh_system(id, mono_br);
      led_unlock();
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Handle auto-transitions for temporary states
    led_lock();
    s_state_frame_count++;
    if ((g_led_state == DRV_LED_WIFI_SUCCESS && s_state_frame_count >= 30) ||
        (g_led_state == DRV_LED_WIFI_FAIL && s_state_frame_count >= 30) ||
        (g_led_state == DRV_LED_IR_SUCCESS && s_state_frame_count >= 10) ||
        (g_led_state == DRV_LED_IR_FAIL && s_state_frame_count >= 10)) {
      g_led_state = DRV_LED_IDLE;
      for (int id = 0; id < MAX_LEDS; id++) {
        s_leds[id].dirty = true;
      }
    }
    led_unlock();
  }
}

esp_err_t drv_led_init(void) {
  if (!s_led_lock) {
    s_led_lock = xSemaphoreCreateMutex();
    if (!s_led_lock) return ESP_ERR_NO_MEM;
  }

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
      led_strip_config_t strip_config = {
          .strip_gpio_num = s_leds[id].gpio,
          .max_leds = s_leds[id].max_leds,
          .led_model = LED_MODEL_WS2812,
          .color_component_format = led_get_color_format(),
          .flags = {
              .invert_out = false,
          },
      };
      led_strip_rmt_config_t rmt_config = {
          .clk_src = RMT_CLK_SRC_DEFAULT,
          .resolution_hz = 10 * 1000 * 1000,
          .mem_block_symbols = 64,
          .flags = {
              .with_dma = false,
          },
      };
      ESP_LOGI(TAG, "Init LED strip %d: GPIO=%d, LEDs=%d, max_output=%d/255",
               id, s_leds[id].gpio, s_leds[id].max_leds, CONFIG_APP_LED_STRIP_MAX_OUTPUT);
      esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_leds[id].strip);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip %d on GPIO %d: %s", id, s_leds[id].gpio, esp_err_to_name(ret));
        continue;
      }
      led_strip_clear(s_leds[id].strip);
      led_strip_refresh(s_leds[id].strip);
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
    s_leds[id].r = 0; s_leds[id].g = 255; s_leds[id].b = 0;
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

esp_err_t drv_led_set_state(drv_led_state_t state) {
  g_led_state = state;
  for (int id = 0; id < MAX_LEDS; id++) {
    s_leds[id].dirty = true;
    drv_led_apply(id);
  }
  return ESP_OK;
}
void drv_led_set_state_isr(drv_led_state_t state) {
  g_led_state = state;
  for (int id = 0; id < MAX_LEDS; id++) {
    s_leds[id].dirty = true;
  }
}

void drv_led_begin_update(uint8_t id) {
  if (id >= MAX_LEDS) return;
  s_leds[id].batch_mode = true;
}

void drv_led_commit_update(uint8_t id) {
  if (id >= MAX_LEDS) return;
  s_leds[id].batch_mode = false;
  s_leds[id].dirty = true;
  drv_led_apply(id);
  drv_led_trigger_bridge_update(id);
}

esp_err_t drv_led_set_base_color(uint8_t id, uint8_t r, uint8_t g, uint8_t b) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].r = r; s_leds[id].g = g; s_leds[id].b = b;
  s_leds[id].dirty = true;
  if (!s_leds[id].batch_mode) {
    drv_led_apply(id);
    drv_led_trigger_bridge_update(id);
  }
  return ESP_OK;
}
esp_err_t drv_led_set_color(uint8_t id, uint8_t r, uint8_t g, uint8_t b) { return drv_led_set_base_color(id, r, g, b); }
esp_err_t drv_led_set_effect(uint8_t id, drv_led_effect_t effect) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].effect = effect;
  s_leds[id].dirty = true;
  if (!s_leds[id].batch_mode) {
    drv_led_apply(id);
    drv_led_trigger_bridge_update(id);
  }
  return ESP_OK;
}
esp_err_t drv_led_set_brightness(uint8_t id, uint8_t brightness) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].brightness = brightness > 100 ? 100 : brightness;
  s_leds[id].dirty = true;
  if (!s_leds[id].batch_mode) {
    drv_led_apply(id);
    drv_led_trigger_bridge_update(id);
  }
  return ESP_OK;
}
esp_err_t drv_led_set_speed(uint8_t id, uint8_t speed) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].speed = speed;
  s_leds[id].dirty = true;
  if (!s_leds[id].batch_mode) {
    drv_led_apply(id);
    drv_led_trigger_bridge_update(id);
  }
  return ESP_OK;
}
esp_err_t drv_led_set_power(uint8_t id, uint8_t power) {
  if (id >= MAX_LEDS) return ESP_FAIL;
  s_leds[id].power = power;
  s_leds[id].dirty = true;
  if (!s_leds[id].batch_mode) {
    drv_led_apply(id);
    drv_led_trigger_bridge_update(id);
  }
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
