# Component: WiFi Service (svc_wifi)

## 1. Tổng quan (Overview)
Component này quản lý tầng driver WiFi của ESP32 ở chế độ **Station (STA)**. Nó xử lý việc kết nối vào Router, quản lý sự kiện (Events) như mất kết nối, tự động kết nối lại và cấp phát địa chỉ IP.

## 2. Tính năng (Features)
- Khởi tạo ngăn xếp WiFi (LwIP).
- Kết nối vào SSID và mật khẩu đã lưu.
- Cơ chế quét mạng (Scan) để tìm kiếm các AP xung quanh.
- Quản lý trạng thái kết nối và báo cáo cho các service khác qua event loop.

## 3. Phụ thuộc (Dependencies)
- `esp_wifi`: Driver WiFi của ESP-IDF.
- `esp_event`: Hệ thống xử lý sự kiện.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "svc_wifi.h"

void app_main(void) {
    // 1. Khởi tạo driver
    svc_wifi_init();

    // 2. Bắt đầu kết nối (có thể truyền PoP cho provisioning)
    svc_wifi_start(NULL);

    // 3. Kiểm tra trạng thái
    if (svc_wifi_is_connected()) {
        printf("WiFi Connected!\n");
    }
}
```

## 5. API chính (Main API)

### `void svc_wifi_init(void)`
Khởi tạo cấu trúc dữ liệu WiFi, đăng ký event handler cho WiFi và IP events.

### `esp_err_t svc_wifi_start(void *pop_info)`
Bắt đầu tiến trình kết nối. Nếu chưa có thông tin WiFi, nó có thể kích hoạt các tiến trình provisioning (tùy cấu hình).

### `bool svc_wifi_is_connected(void)`
Trả về trạng thái kết nối hiện tại của Station.

### `esp_err_t svc_wifi_get_scan_results(wifi_ap_record_t **ap_list, uint16_t *count)`
Thực hiện quét các mạng WiFi xung quanh và trả về danh sách kết quả.

## 6. Logic kết nối lại (Reconnection)
Service này tự động thực hiện nỗ lực kết nối lại khi bị mất kết nối (do Router reset, mất điện...). Khoảng thời gian thử lại tăng dần để tránh làm phiền hệ thống.
