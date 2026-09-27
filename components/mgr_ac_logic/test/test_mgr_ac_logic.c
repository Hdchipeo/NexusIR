#include "esp_log.h"
#include "mgr_ac_logic.h"
#include "nvs_flash.h"
#include "unity.h"
#include <string.h>

static void init_nvs(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

void setUp(void) {
  init_nvs();
  mgr_ac_init();
}

void tearDown(void) {
  // Optional: Clean up NVS or reset state
}

TEST_CASE("AC Brand Set/Get", "[mgr_ac_logic]") {
  mgr_ac_set_brand(AC_BRAND_SAMSUNG);
  TEST_ASSERT_EQUAL(AC_BRAND_SAMSUNG, mgr_ac_get_brand());
  TEST_ASSERT_FALSE(mgr_ac_is_custom_brand());

  mgr_ac_set_brand(AC_BRAND_DAIKIN);
  TEST_ASSERT_EQUAL(AC_BRAND_DAIKIN, mgr_ac_get_brand());
}

TEST_CASE("AC Custom Brand Set/Get", "[mgr_ac_logic]") {
  const char *custom_name = "MyCustomAC";
  mgr_ac_set_custom_brand(custom_name);

  TEST_ASSERT_TRUE(mgr_ac_is_custom_brand());
  TEST_ASSERT_EQUAL_STRING(custom_name, mgr_ac_get_custom_brand());

  // Setting standard brand should clear custom flag
  mgr_ac_set_brand(AC_BRAND_LG);
  TEST_ASSERT_FALSE(mgr_ac_is_custom_brand());
  TEST_ASSERT_NULL(mgr_ac_get_custom_brand());
}

TEST_CASE("AC State Set/Get", "[mgr_ac_logic]") {
  ir_ac_state_t set_state = {.power = true,
                             .mode = 2, // Dry
                             .temp = 25,
                             .fan = 1,
                             .swing_v = 1,
                             .swing_h = 0};

  mgr_ac_set_state(&set_state);

  ir_ac_state_t get_state;
  mgr_ac_get_state(&get_state);

  TEST_ASSERT_TRUE(get_state.power);
  TEST_ASSERT_EQUAL(2, get_state.mode);
  TEST_ASSERT_EQUAL(25, get_state.temp);
  TEST_ASSERT_EQUAL(1, get_state.fan);
  TEST_ASSERT_EQUAL(1, get_state.swing_v);
}
