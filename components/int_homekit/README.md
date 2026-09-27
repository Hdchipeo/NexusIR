# Component: HomeKit Integration (int_homekit)

## 1. Tổng quan (Overview)
Component này tích hợp bộ thư viện **Apple HomeKit (HAP)** vào hệ thống. Nó khai báo các Accessory (Thiết bị) và Service (Dịch vụ) tương ứng để người dùng có thể điều khiển NexusIR thông qua ứng dụng Nhà (Home) trên iPhone, iPad, Mac hoặc qua Siri.

## 2. Các thiết bị hỗ trợ (Supported Accessories)
- **Air Conditioner:** Điều khiển nguồn, nhiệt độ, chế độ (Heat/Cool/Auto).
- **Fan:** Điều khiển nguồn và tốc độ quạt.
- **Lightbulb:** Điều khiển tối đa 5 dải LED (Nguồn, Độ sáng, Màu sắc).
- **Switch:** Điều khiển 2 Relay vật lý.
- **Sensors:** Hiển thị nhiệt độ và độ ẩm từ AHT20 hoặc từ remote sensor qua ESP-NOW.

## 3. Phụ thuộc (Dependencies)
- `esp-homekit-sdk`: SDK chính thức từ Espressif cho HomeKit.
- `mgr_ac_logic`, `mgr_fan_logic`, `mgr_led_strip`, `mgr_relay`: Để đồng bộ trạng thái giữa HomeKit và logic thực tế.
- `svc_wifi_prov`: Để xử lý trạng thái WiFi và Provisioning.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "int_homekit.h"

void app_main(void) {
    // 1. Khởi tạo server HomeKit
    int_homekit_init();

    // 2. Khi trạng thái thiết bị thay đổi cục bộ (ví dụ Web UI), cập nhật lên HomeKit
    ir_ac_state_t ac_state;
    // ... get ac_state ...
    int_homekit_update_state(&ac_state);
}
```

## 5. API chính (Main API)

### `esp_err_t int_homekit_init(void)`
Khởi tạo cấu trúc dữ liệu Accessory, các Characteristic (đặc tính) và bắt đầu Accessory Server.

### `void int_homekit_update_state(const ir_ac_state_t *state)`
Đồng bộ trạng thái AC từ hệ thống lên HomeKit UI.

### `bool int_homekit_is_paired(void)`
Kiểm tra xem thiết bị đã được "Pair" (Ghép đôi) với một Home Hub nào chưa.

## 6. Thiết lập (Provisioning)
Khi mới sử dụng, thiết bị sẽ phát ra mã setup HomeKit (mặc định cấu hình trong `app_hap_setup_payload`). Người dùng quét mã QR hoặc nhập mã PIN trên ứng dụng Home để thêm thiết bị.
