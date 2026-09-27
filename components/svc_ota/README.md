# Component: OTA Manager Service (svc_ota)

## 1. Tổng quan (Overview)
Component này quản lý việc cập nhật phần mềm qua mạng (**Over-The-Air - OTA**). Nó hỗ trợ kiểm tra phiên bản mới từ máy chủ, tải xuống bản cập nhật qua giao thức HTTPS bảo mật và tự động khởi động lại để áp dụng bản nâng cấp.

## 2. Tính năng (Features)
- Kiểm tra phiên bản định kỳ (Background task).
- Cập nhật firmware qua HTTP/HTTPS.
- Cơ chế Rollback: Nếu firmware mới không thể kết nối mạng hoặc bị crash, hệ thống sẽ tự động quay về phiên bản cũ ổn định.
- Lưu trữ lịch sử cập nhật.

## 3. Phụ thuộc (Dependencies)
- `esp_https_ota`: Driver OTA chính thức của ESP-IDF.
- `svc_wifi`: Yêu cầu kết nối mạng ổn định.

## 4. Cấu hình (Configuration)
Cấu hình trong `menuconfig` -> `OTA Configuration`:
- `CONFIG_OTA_SERVER_URL`: Địa chỉ máy chủ chứa file firmware.
- `CONFIG_OTA_CHECK_INTERVAL`: Khoảng thời gian giữa các lần kiểm tra (giây).

## 5. Hướng dẫn sử dụng (Usage Example)

```c
#include "svc_ota.h"

void app_main(void) {
    // 1. Khởi tạo tự động kiểm tra
    svc_ota_auto_init();

    // 2. Kiểm tra thủ công (nếu cần)
    if (svc_ota_is_update_available()) {
        printf("New version found: %s\n", svc_ota_get_cached_version());
        // svc_ota_start(url);
    }
    
    // 3. Đánh dấu boot thành công để tránh rollback
    svc_ota_mark_valid();
}
```

## 6. API chính (Main API)

### `void svc_ota_auto_init(void)`
Bắt đầu một task chạy ngầm định kỳ gửi yêu cầu tới server để so sánh phiên bản hiện tại với phiên bản trên server.

### `esp_err_t svc_ota_start(const char *url)`
Bắt đầu quá trình tải và nạp firmware từ `url` chỉ định. Đây là hàm blocking.

### `void svc_ota_mark_valid(void)`
Xác nhận ứng dụng hiện tại đang hoạt động tốt. Nên gọi sau khi WiFi đã kết nối thành công. Nếu không gọi hàm này, ESP32 có thể tự quay về bản cũ sau khi reset.

## 7. Quy trình cập nhật (Process)
1. ESP32 tải file `version.json` từ server.
2. So sánh chuỗi phiên bản. Nếu khác nhau -> Có bản cập nhật.
3. Tải file `.bin` -> Ghi vào phân vùng OTA trống.
4. Kiểm tra mã băm (checksum).
5. Đổi cờ khởi động (Boot partition) và restart.
