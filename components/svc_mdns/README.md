# Component: mDNS Service (svc_mdns)

## 1. Tổng quan (Overview)
Component này triển khai giao thức **Multicast DNS (mDNS)**, cho phép người dùng truy cập vào Web UI của thiết bị thông qua một tên miền thân thiện (ví dụ: `http://nexusir.local`) thay vì phải nhớ địa chỉ IP động do Router cấp.

## 2. Tính năng (Features)
- Phân giải tên miền cục bộ `.local`.
- Quảng bá các dịch vụ đang chạy (HTTP, HomeKit).
- Giúp thiết bị dễ dàng được tìm thấy bởi ứng dụng di động hoặc trình duyệt trong cùng mạng LAN.

## 3. Phụ thuộc (Dependencies)
- `mdns`: Thành phần chính thức của ESP-IDF.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "svc_mdns.h"

void app_main(void) {
    // Gọi sau khi WiFi đã kết nối thành công
    svc_mdns_init();
}
```

## 5. API chính (Main API)

### `esp_err_t svc_mdns_init(void)`
Khởi tạo mDNS stack, thiết lập hostname (mặc định dựa trên MAC address hoặc "nexusir") và đăng ký dịch vụ HTTP (cổng 80).

## 6. Lưu ý
- Tên miền mặc định thường là `nexusir.local`.
- Một số hệ điều hành cũ (như Windows 7) có thể cần cài đặt thêm "Bonjour Service" để nhận diện được tên miền mDNS. Windows 10/11, macOS, Android và iOS hỗ trợ sẵn.
