# Component: WiFi Provisioning Service (svc_wifi_prov)

## 1. Tổng quan (Overview)
Component này cung cấp giải pháp cấu hình WiFi thông qua **Captive Portal** (Trang chào). Khi thiết bị chưa được cấu hình WiFi, nó sẽ phát ra một điểm truy cập WiFi (SoftAP) riêng. Người dùng kết nối vào WiFi này, một trang web cấu hình sẽ tự động hiện ra để nhập SSID và mật khẩu.

## 2. Tính năng (Features)
- Tự động kích hoạt SoftAP khi không có thông tin WiFi trong NVS.
- Captive Portal: Tự động chuyển hướng trình duyệt tới trang cấu hình (DNS Redirect).
- Giao diện Web đơn giản, dễ sử dụng trên điện thoại.
- Lưu trữ thông tin WiFi vào NVS một cách bảo mật.

## 3. Phụ thuộc (Dependencies)
- `svc_wifi`: Để quản lý driver WiFi.
- `svc_web_server`: Để chạy máy chủ web cấu hình.
- `svc_nvs`: Để lưu trữ credentials.

## 4. Cấu hình (Configuration)
Cấu hình trong `menuconfig` -> `iOS WiFi Provisioning`:
- `IOS_WIFI_AP_SSID_PREFIX`: Tiền tố tên WiFi của thiết bị (mặc định: `NexusIR-Setup`).
- `IOS_WIFI_AP_PASSWORD`: Mật khẩu của WiFi thiết bị (để trống nếu muốn mở).

## 5. Hướng dẫn sử dụng (Usage Example)

```c
#include "svc_wifi_prov.h"

void app_main(void) {
    svc_wifi_prov_init();

    if (!svc_wifi_prov_creds_exist()) {
        // Chưa có WiFi, bắt đầu phát AP để cấu hình
        svc_wifi_prov_start();
    } else {
        // Đã có WiFi, tiến hành kết nối bình thường
        char ssid[33], pwd[65];
        svc_wifi_prov_creds_load(ssid, pwd);
        // ... connect ...
    }
}
```

## 6. API chính (Main API)

### `esp_err_t svc_wifi_prov_start(void)`
Kích hoạt SoftAP, DNS Server (để redirect) và khởi chạy Web Server chứa trang cấu hình.

### `void svc_wifi_prov_stop(void)`
Dừng SoftAP và giải phóng tài nguyên.

### `bool svc_wifi_prov_creds_exist(void)`
Kiểm tra xem trong bộ nhớ NVS đã có thông tin WiFi hợp lệ hay chưa.

### `esp_err_t svc_wifi_prov_creds_save(const char *ssid, const char *password)`
Lưu thông tin WiFi mới vào NVS.

## 7. Quy trình người dùng (User Flow)
1. Cắm điện thiết bị mới.
2. Dùng điện thoại tìm WiFi tên `NexusIR-Setup-XXXX`.
3. Kết nối vào và trình duyệt tự bật lên (hoặc truy cập `192.168.4.1`).
4. Chọn mạng WiFi nhà mình, nhập mật khẩu và nhấn "Save".
5. Thiết bị tự restart và kết nối vào WiFi nhà.
