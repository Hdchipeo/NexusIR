#include "mgr_display.h"

#if CONFIG_APP_LCD_ENABLE

#include "esp_log.h"
#include "lvgl.h"
#include <math.h>
#include <time.h>

static lv_obj_t *main_screen;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_hum;
static lv_obj_t *lbl_status;
static lv_obj_t *sun_obj;
static lv_obj_t *cloud_base_obj;
static lv_obj_t *cloud_puff_obj;
static lv_obj_t *rain_obj[3];
static lv_obj_t *lightning_obj;
static lv_obj_t *snow_obj[3];

static lv_obj_t *notification_cont;
static lv_obj_t *lbl_notify_title;
static lv_obj_t *lbl_notify_status;
static lv_timer_t *notify_timer = NULL;

static void notify_timer_cb(lv_timer_t *timer) {
  lv_obj_add_flag(notification_cont, LV_OBJ_FLAG_HIDDEN);
  lv_timer_del(timer);
  notify_timer = NULL;
}

void mgr_display_ui_init(void) {
  main_screen = lv_scr_act();
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);

  // Weather Status (Top)
  lbl_status = lv_label_create(main_screen);
  lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18,
                             0); // Decreased size
  lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(lbl_status, "Sunny day");
  lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 25);

  // Weather Icon Container - Larger and Centered
  lv_obj_t *icon_cont = lv_obj_create(main_screen);
  lv_obj_set_size(icon_cont, 110, 110);
  lv_obj_set_style_bg_opa(icon_cont, 0, 0);
  lv_obj_set_style_border_width(icon_cont, 0, 0);
  lv_obj_align(icon_cont, LV_ALIGN_CENTER, 0, -15);

  // Sun
  sun_obj = lv_obj_create(icon_cont);
  lv_obj_set_size(sun_obj, 50, 50);
  lv_obj_set_style_radius(sun_obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(sun_obj, lv_color_hex(0xFFD700), 0);
  lv_obj_set_style_border_width(sun_obj, 0, 0);
  lv_obj_align(sun_obj, LV_ALIGN_CENTER, 15, -10);

  // Cloud
  cloud_base_obj = lv_obj_create(icon_cont);
  lv_obj_set_size(cloud_base_obj, 60, 30);
  lv_obj_set_style_radius(cloud_base_obj, 15, 0);
  lv_obj_set_style_bg_color(cloud_base_obj, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(cloud_base_obj, 0, 0);
  lv_obj_align(cloud_base_obj, LV_ALIGN_CENTER, -10, 15);

  cloud_puff_obj = lv_obj_create(icon_cont);
  lv_obj_set_size(cloud_puff_obj, 35, 35);
  lv_obj_set_style_radius(cloud_puff_obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cloud_puff_obj, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(cloud_puff_obj, 0, 0);
  lv_obj_align(cloud_puff_obj, LV_ALIGN_CENTER, -15, 5);

  // Rain Drops
  for (int i = 0; i < 3; i++) {
    rain_obj[i] = lv_obj_create(icon_cont);
    lv_obj_set_size(rain_obj[i], 4, 12);
    lv_obj_set_style_bg_color(rain_obj[i], lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_border_width(rain_obj[i], 0, 0);
    lv_obj_align(rain_obj[i], LV_ALIGN_CENTER, -25 + (i * 20), 35);
    lv_obj_add_flag(rain_obj[i], LV_OBJ_FLAG_HIDDEN);
  }

  // Lightning
  lightning_obj = lv_obj_create(icon_cont);
  lv_obj_set_size(lightning_obj, 10, 25);
  lv_obj_set_style_bg_color(lightning_obj, lv_color_hex(0xFFFF00), 0);
  lv_obj_set_style_border_width(lightning_obj, 0, 0);
  lv_obj_set_style_radius(lightning_obj, 2, 0);
  lv_obj_align(lightning_obj, LV_ALIGN_CENTER, 0, 30);
  lv_obj_add_flag(lightning_obj, LV_OBJ_FLAG_HIDDEN);

  // Snow
  for (int i = 0; i < 3; i++) {
    snow_obj[i] = lv_obj_create(icon_cont);
    lv_obj_set_size(snow_obj[i], 6, 6);
    lv_obj_set_style_radius(snow_obj[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(snow_obj[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(snow_obj[i], 0, 0);
    lv_obj_align(snow_obj[i], LV_ALIGN_CENTER, -25 + (i * 20), 35);
    lv_obj_add_flag(snow_obj[i], LV_OBJ_FLAG_HIDDEN);
  }

  // Temp Label
  lbl_temp = lv_label_create(main_screen);
  lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_28,
                             0); // Decreased size
  lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(lbl_temp, "--°C");
  lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, 50);

  // Humidity Label (Increased size, no time)
  lbl_hum = lv_label_create(main_screen);
  lv_obj_set_style_text_font(lbl_hum, &lv_font_montserrat_14,
                             0); // Increased size
  lv_obj_set_style_text_color(lbl_hum, lv_color_hex(0x2196F3),
                              0); // Blue for humidity
  lv_label_set_text(lbl_hum, "H: --%");
  lv_obj_align(lbl_hum, LV_ALIGN_BOTTOM_MID, 0, -20);

  // Notification Container
  notification_cont = lv_obj_create(main_screen);
  lv_obj_set_size(notification_cont, 180, 80);
  lv_obj_set_style_bg_color(notification_cont, lv_color_hex(0x1E1E1E), 0);
  lv_obj_set_style_border_width(notification_cont, 1, 0);
  lv_obj_set_style_border_color(notification_cont, lv_color_hex(0x00FFCC), 0);
  lv_obj_set_style_radius(notification_cont, 15, 0);
  lv_obj_align(notification_cont, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(notification_cont, LV_OBJ_FLAG_HIDDEN);

  lbl_notify_title = lv_label_create(notification_cont);
  lv_obj_set_style_text_font(lbl_notify_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_notify_title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(lbl_notify_title, LV_ALIGN_TOP_MID, 0, 10);

  lbl_notify_status = lv_label_create(notification_cont);
  lv_obj_set_style_text_font(lbl_notify_status, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(lbl_notify_status, lv_color_hex(0x00FFCC), 0);
  lv_obj_align(lbl_notify_status, LV_ALIGN_CENTER, 0, 15);
}

static bool s_is_day = false;

static void update_theme(bool is_day) {
  if (s_is_day == is_day)
    return;
  s_is_day = is_day;

  lv_color_t bg_color = is_day ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
  lv_color_t fg_color = is_day ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);

  lv_obj_set_style_bg_color(main_screen, bg_color, 0);
  lv_obj_set_style_text_color(lbl_status, fg_color, 0);
  lv_obj_set_style_text_color(lbl_temp, fg_color, 0);

  // Cloud needs better contrast on white bg (Day mode)
  lv_color_t cloud_color =
      is_day ? lv_color_hex(0x999999) : lv_color_hex(0xFFFFFF);
  if (cloud_base_obj)
    lv_obj_set_style_bg_color(cloud_base_obj, cloud_color, 0);
  if (cloud_puff_obj)
    lv_obj_set_style_bg_color(cloud_puff_obj, cloud_color, 0);

  // Snow also needs contrast
  lv_color_t snow_color =
      is_day ? lv_color_hex(0x999999) : lv_color_hex(0xFFFFFF);
  for (int i = 0; i < 3; i++) {
    if (snow_obj[i])
      lv_obj_set_style_bg_color(snow_obj[i], snow_color, 0);
  }
}

void mgr_display_update_ui_time(void) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  // Day: 6 AM to 6 PM (18:00)
  bool is_day = (timeinfo.tm_hour >= 6 && timeinfo.tm_hour < 18);
  update_theme(is_day);
}

void mgr_display_update_ui_weather(const char *status) {
  if (status == NULL)
    return;
  lv_label_set_text(lbl_status, status);
}

void mgr_display_update_ui_weather_code(int code) {
  // Hide everything first
  lv_obj_add_flag(sun_obj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(cloud_base_obj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(cloud_puff_obj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(lightning_obj, LV_OBJ_FLAG_HIDDEN);
  for (int i = 0; i < 3; i++) {
    lv_obj_add_flag(rain_obj[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(snow_obj[i], LV_OBJ_FLAG_HIDDEN);
  }

  // Show based on code (Open-Meteo codes)
  if (code == 0) { // Sunny
    lv_obj_clear_flag(sun_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(sun_obj, LV_ALIGN_CENTER, 0, 0);
  } else if (code >= 1 && code <= 3) { // Partly Cloudy
    lv_obj_clear_flag(sun_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cloud_base_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cloud_puff_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(sun_obj, LV_ALIGN_CENTER, 15, -10);
  } else if (code == 45 || code == 48) { // Foggy
    lv_obj_clear_flag(cloud_base_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cloud_puff_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(cloud_base_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(cloud_puff_obj, LV_ALIGN_CENTER, -5, -10);
  } else if (code >= 51 && code <= 65) { // Rainy
    lv_obj_clear_flag(cloud_base_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cloud_puff_obj, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 3; i++)
      lv_obj_clear_flag(rain_obj[i], LV_OBJ_FLAG_HIDDEN);
  } else if (code >= 71 && code <= 77) { // Snowy
    lv_obj_clear_flag(cloud_base_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cloud_puff_obj, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 3; i++)
      lv_obj_clear_flag(snow_obj[i], LV_OBJ_FLAG_HIDDEN);
  } else if (code >= 95) { // Stormy
    lv_obj_clear_flag(cloud_base_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cloud_puff_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lightning_obj, LV_OBJ_FLAG_HIDDEN);
  }
}

void mgr_display_update_ui_sensor(float temp, float hum) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f°C", temp);
  lv_label_set_text(lbl_temp, buf);

  // Dynamic Status based on temperature
  if (temp > 32)
    lv_label_set_text(lbl_status, "Hot day");
  else if (temp > 25)
    lv_label_set_text(lbl_status, "Sunny day");
  else if (temp > 18)
    lv_label_set_text(lbl_status, "Nice day");
  else
    lv_label_set_text(lbl_status, "Cool day");

  snprintf(buf, sizeof(buf), "H: %.0f%%", hum);
  lv_label_set_text(lbl_hum, buf);
}

void mgr_display_show_ui_notification(const char *device_name,
                                      const char *status) {
  lv_label_set_text(lbl_notify_title, device_name);
  lv_label_set_text(lbl_notify_status, status);
  lv_obj_clear_flag(notification_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(notification_cont);

  if (notify_timer) {
    lv_timer_reset(notify_timer);
  } else {
    notify_timer = lv_timer_create(notify_timer_cb, 3000, NULL);
  }
}
#endif // CONFIG_APP_LCD_ENABLE
