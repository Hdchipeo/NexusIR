#include "svc_log.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_BUFFER_SIZE 4096

static char s_log_buffer[LOG_BUFFER_SIZE];
static size_t s_log_head = 0; // Write index
static bool s_log_wrapped = false;
static SemaphoreHandle_t s_log_mutex = NULL;
static vprintf_like_t s_prev_vprintf = NULL;

static int svc_log_vprintf(const char *fmt, va_list ap) {
  // If in ISR, bypass the ring buffer logic to save stack and avoid mutex.
  if (xPortInIsrContext()) {
    if (s_prev_vprintf) {
      return s_prev_vprintf(fmt, ap);
    }
    return vprintf(fmt, ap);
  }

  // First, print to standard output (UART/USB)
  // We must va_copy because va_list is consumed after first use
  va_list ap_copy;
  va_copy(ap_copy, ap);

  int ret = 0;
  if (s_prev_vprintf) {
    ret = s_prev_vprintf(fmt, ap);
  } else {
    ret = vprintf(fmt, ap);
  }

  // Capture into ring buffer for Web UI log viewer.
  // Skip if stack is too small (e.g. IDLE task) to avoid overflow.
  // vsnprintf needs ~1KB+ stack; IDLE task only has ~1.5KB total.
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  UBaseType_t stack_remaining = (current_task != NULL)
                                    ? uxTaskGetStackHighWaterMark(current_task)
                                    : 0;

  if (s_log_mutex && stack_remaining > 128 &&
      xSemaphoreTake(s_log_mutex, 0) == pdTRUE) {
    // Use static buffer to avoid consuming caller's stack
    static char temp_buf[256];
    int len = vsnprintf(temp_buf, sizeof(temp_buf), fmt, ap_copy);
    if (len > 0) {
      if (len >= (int)sizeof(temp_buf)) {
        len = sizeof(temp_buf) - 1;
      }
      for (int i = 0; i < len; i++) {
        s_log_buffer[s_log_head++] = temp_buf[i];
        if (s_log_head >= LOG_BUFFER_SIZE) {
          s_log_head = 0;
          s_log_wrapped = true;
        }
      }
    }
    xSemaphoreGive(s_log_mutex);
  }

  va_end(ap_copy);
  return ret;
}

void svc_log_init(void) {
  if (!s_log_mutex) {
    s_log_mutex = xSemaphoreCreateMutex();
    s_prev_vprintf = esp_log_set_vprintf(svc_log_vprintf);
  }
}

int svc_log_get_buffer(char *dest, size_t max_len) {
  int total_len = 0;
  if (s_log_mutex &&
      xSemaphoreTake(s_log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (!s_log_wrapped) {
      size_t copy_len = (s_log_head < max_len) ? s_log_head : max_len - 1;
      memcpy(dest, s_log_buffer, copy_len);
      dest[copy_len] = 0;
      total_len = copy_len;
    } else {
      // Buffer wrapped: Read from head to end, then 0 to head
      // To simplify display, we just linearize the ring buffer into dest
      // Start from s_log_head (oldest data) to end
      size_t part1_len = LOG_BUFFER_SIZE - s_log_head;
      size_t part2_len = s_log_head;

      size_t copied = 0;

      // Copy part 1
      if (part1_len > 0 && copied < max_len - 1) {
        size_t to_copy = (part1_len < (max_len - 1 - copied))
                             ? part1_len
                             : (max_len - 1 - copied);
        memcpy(dest + copied, s_log_buffer + s_log_head, to_copy);
        copied += to_copy;
      }

      // Copy part 2
      if (part2_len > 0 && copied < max_len - 1) {
        size_t to_copy = (part2_len < (max_len - 1 - copied))
                             ? part2_len
                             : (max_len - 1 - copied);
        memcpy(dest + copied, s_log_buffer, to_copy);
        copied += to_copy;
      }
      dest[copied] = 0;
      total_len = copied;
    }
    xSemaphoreGive(s_log_mutex);
  }
  return total_len;
}

void svc_log_clear(void) {
  if (s_log_mutex &&
      xSemaphoreTake(s_log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    s_log_head = 0;
    s_log_wrapped = false;
    memset(s_log_buffer, 0, LOG_BUFFER_SIZE);
    xSemaphoreGive(s_log_mutex);
  }
}
