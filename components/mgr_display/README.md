# Component: Display Manager (mgr_display)

## 1. Tổng quan (Overview)
Component này quản lý màn hình hiển thị (LCD) và giao diện người dùng (UI) dựa trên thư viện **LVGL**. Nó chịu trách nhiệm hiển thị các thông số cảm biến, thời gian, thời tiết và các thông báo hệ thống. Nó cũng quản lý việc tiết kiệm điện bằng cách tự động làm mờ hoặc tắt màn hình sau một khoảng thời gian không hoạt động.

## 2. Phần cứng (Hardware Setup)
Phụ thuộc vào loại board sử dụng (ESP32-S3/C3 với màn hình ST7789 hoặc tương đương). Các chân giao tiếp (SPI/I2C/Parallel) thường được cấu hình trong `menuconfig` của driver màn hình.

## 3. Phụ thuộc (Dependencies)
- `lvgl`: Thư viện đồ họa.
- `esp_lcd`: Driver LCD của ESP-IDF.
- `drv_button`: Để đánh thức màn hình khi nhấn nút.
- `svc_weather`: Để lấy dữ liệu hiển thị thời tiết.

## 4. Cấu hình (Configuration)
Cấu hình trong `menuconfig`:
- `CONFIG_APP_LCD_ENABLE`: Bật/Tắt hỗ trợ màn hình.
- Các thông số Timeout cho việc Dim/Sleep màn hình.

## 5. Hướng dẫn sử dụng (Usage Example)

```c
#include "mgr_display.h"

void app_main(void) {
    // 1. Khởi tạo LCD và LVGL
    mgr_display_init();

    // 2. Cập nhật dữ liệu lên UI (An toàn để gọi từ bất kỳ task nào)
    mgr_display_update_ui_sensor_safe(28.5, 65.0);
    mgr_display_update_ui_time_safe();

    // 3. Thông báo cho display biết có hoạt động (Reset timeout tắt màn hình)
    mgr_display_activity_tick();
}
```

## 6. API chính (Main API)

### `esp_err_t mgr_display_init(void)`
Khởi tạo phần cứng hiển thị, đăng ký buffer cho LVGL và bắt đầu Task quản lý UI.

### `void mgr_display_update_ui_sensor_safe(float temp, float hum)`
Cập nhật nhiệt độ và độ ẩm lên màn hình chính một cách an toàn (sử dụng mutex nếu cần).

### `void mgr_display_activity_tick(void)`
Đánh dấu hệ thống vừa có hoạt động (như nhấn nút, nhận lệnh). Hàm này sẽ bật sáng màn hình nếu đang tối và reset bộ đếm thời gian chờ.

### `void mgr_display_show_ui_notification_safe(const char *device_name, const char *status)`
Hiển thị một thông báo popup ngắn gọn trên màn hình (ví dụ: "AC Turned ON").

## 7. Cơ chế tiết kiệm điện (Power Management)
- Sau 30s không hoạt động: Màn hình giảm độ sáng (Dim).
- Sau 60s không hoạt động: Màn hình tắt hẳn (Sleep/Off).
- Nhấn nút hoặc có sự kiện mới: Màn hình sáng trở lại (Wake).
