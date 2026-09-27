# Component: HomeKit Setup Payload (app_hap_setup_payload)

## 1. Tổng quan (Overview)
Component này chịu trách nhiệm tạo ra các thông tin cần thiết để người dùng có thể thêm thiết bị vào ứng dụng Nhà (Home) của Apple. Nó xử lý việc tạo mã Setup Code, Setup ID và quản lý trạng thái ghép đôi (Pairing).

## 2. Tính năng (Features)
- Định nghĩa mã PIN HomeKit (mặc định hoặc từ cấu hình).
- Phân loại loại thiết bị (Category) cho HomeKit (e.g. Bridge, Air Conditioner, Fan).
- Cung cấp thông tin để tạo mã QR Code hoặc mã PIN 8 số.

## 3. Phụ thuộc (Dependencies)
- `esp-homekit-sdk`: SDK HomeKit của Espressif.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "app_hap_setup_payload.h"

void app_main(void) {
    char setup_code[10]; // e.g. "111-22-333"
    char setup_id[5];    // e.g. "NXIR"
    
    // Thường được gọi bởi int_homekit để chuẩn bị payload
    app_hap_setup_payload(setup_code, setup_id, false, 2); // Category 2 = Bridge
}
```

## 5. API chính (Main API)

### `void app_hap_setup_payload(char *setup_code, char *setup_id, bool paired, uint8_t category)`
Thiết lập các tham số định danh cho thiết bị trong mạng HomeKit.
- `setup_code`: Mã PIN 8 số dùng để nhập tay.
- `setup_id`: Mã ID dùng cho QR code.
- `paired`: Trạng thái đã ghép đôi hay chưa.
- `category`: Loại thiết bị theo định nghĩa của Apple.
