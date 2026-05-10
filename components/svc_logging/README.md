# Component: Logging Service (svc_logging)

## 1. Tổng quan (Overview)
Component này cung cấp một hệ thống lưu trữ log hệ thống vào bộ nhớ tạm (In-memory buffer). Mục đích là để người dùng có thể xem lại các sự kiện, lỗi hệ thống thông qua giao diện Web UI mà không cần phải kết nối thiết bị với máy tính qua cổng Serial.

## 2. Tính năng (Features)
- Thu thập log từ `ESP_LOGX`.
- Lưu trữ log trong một Ring Buffer cố định để tránh tràn bộ nhớ.
- Cung cấp API để lấy toàn bộ nội dung log dưới dạng text.

## 3. Phụ thuộc (Dependencies)
- `esp_log`: Tích hợp trực tiếp với hệ thống logging của ESP-IDF.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "svc_log.h"

void app_main(void) {
    // Khởi tạo service
    svc_log_init();
    
    // Sử dụng ESP_LOG thông thường, dữ liệu sẽ tự động được copy vào buffer
    ESP_LOGI("APP", "System Started");
}

// Trong code Web Server:
void get_logs_handler() {
    char log_buf[2048];
    svc_log_get_buffer(log_buf, sizeof(log_buf));
    // Gửi log_buf về trình duyệt...
}
```

## 5. API chính (Main API)

### `void svc_log_init(void)`
Khởi tạo vùng nhớ đệm cho log và đăng ký hook vào hệ thống logging.

### `int svc_log_get_buffer(char *dest, size_t max_len)`
Copy nội dung log hiện có vào chuỗi `dest`.

### `void svc_log_clear(void)`
Xóa sạch bộ nhớ đệm log.
