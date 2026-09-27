# Component: Web Server Service (svc_web_server)

## 1. Tổng quan (Overview)
Component này cung cấp một máy chủ HTTP tích hợp, đóng vai trò là giao diện điều khiển chính của NexusIR. Người dùng có thể truy cập qua trình duyệt để điều khiển thiết bị, quản lý mã lệnh hồng ngoại, xem log và cấu hình hệ thống mà không cần cài đặt ứng dụng.

## 2. Tính năng (Features)
- Cung cấp giao diện Web (HTML/CSS/JS) hiện đại.
- Hỗ trợ RESTful API cho các tác vụ:
    - Điều khiển AC, Fan, LED, Relay.
    - Quản lý Matrix IR (Học lệnh, xóa, đổi tên).
    - Xem Log hệ thống thời gian thực.
    - Cập nhật WiFi và xem thông tin hệ thống.
- Tích hợp Captive Portal cho việc cấu hình WiFi lần đầu.

## 3. Phụ thuộc (Dependencies)
- `esp_http_server`: Driver chính thức của ESP-IDF.
- `spiffs`: Chứa các file tĩnh (index.html, css, js).
- `mgr_*`, `svc_*`: Để tương tác với toàn bộ hệ thống.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "svc_web_server.h"

void app_main(void) {
    // 1. Khởi tạo và đăng ký các URI handlers
    svc_web_init();

    // 2. Bắt đầu server (thường gọi sau khi có IP)
    svc_web_start();
}
```

## 5. API chính (Main API)

### `esp_err_t svc_web_init(void)`
Khởi tạo cấu trúc server và đăng ký tất cả các điểm cuối (Endpoints) như `/api/control`, `/api/logs`, `/api/ir_matrix`...

### `esp_err_t svc_web_start(void)`
Thực sự khởi động socket lắng nghe trên cổng 80.

### `void svc_web_stop(void)`
Dừng server và giải phóng tài nguyên.

## 6. Endpoints quan trọng
- `GET /`: Trả về giao diện người dùng chính.
- `POST /api/ac/control`: Điều khiển điều hòa.
- `GET /api/system/logs`: Trả về dữ liệu log từ `svc_logging`.
- `POST /api/ir/learn`: Kích hoạt chế độ học lệnh IR.
