/*
 * Test Runner for Lamp IR Device
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

// If you have many components, you can register them here or rely on the linker
// to pick up tests registered via UNITY_BEGIN/EMS/etc if using the helper
// macros. However, standard Unity in IDF often requires calling
// unity_run_menu() or similar.

void app_main(void) {
  // Wait for monitor to reconnect
  vTaskDelay(pdMS_TO_TICKS(2000));

  printf("\n ================================================= \n");
  printf("           Lamp IR Device - Unit Tests             \n");
  printf(" ================================================= \n\n");

  UNITY_BEGIN();

  // Unity in ESP-IDF usually auto-discovers tests if using
  // TEST_CASE in components. We trigger them here.
  unity_run_all_tests();

  UNITY_END();

  printf("\nTests Completed.\n");
}
