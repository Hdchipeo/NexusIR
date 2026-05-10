# Component: RainMaker Integration (int_rainmaker)

## 1. Tổng quan (Overview)
Component này cung cấp khả năng kết nối Cloud thông qua giải pháp **ESP RainMaker**. Nó cho phép điều khiển toàn bộ các tính năng của NexusIR (AC, Fan, LED, Relay, Sensors) từ xa qua Internet bằng ứng dụng di động ESP RainMaker (có sẵn trên iOS và Android).

## 2. Tính năng (Features)
- Tự động tạo Device Node trên Cloud.
- Hỗ trợ cập nhật trạng thái thời gian thực (Push notification).
- Tích hợp sẵn cơ chế Provisioning (BLE/SoftAP) thông qua RainMaker app.
- Quản lý danh sách các thương hiệu AC/Fan tùy chỉnh.

## 3. Phụ thuộc (Dependencies)
- `esp_rainmaker`: SDK Cloud của Espressif.
- `mgr_*`: Các manager logic để điều khiển thiết bị cục bộ.
- `svc_wifi_prov`: Để xử lý việc cấu hình WiFi.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "int_rainmaker.h"

void app_main(void) {
    // 1. Khởi tạo kết nối RainMaker
    app_rainmaker_init();

    // 2. Cập nhật trạng thái AC lên Cloud khi có thay đổi
    ir_ac_state_t state;
    // ... update state ...
    app_rainmaker_update_ac(&state);
}
```

## 5. API chính (Main API)

### `esp_err_t app_rainmaker_init(void)`
Khởi tạo RainMaker node, khai báo các thiết bị (Devices) và tham số (Params). Bắt đầu tiến trình kết nối MQTT đến server Cloud.

### `void app_rainmaker_update_ac(const ir_ac_state_t *state)`
Gửi toàn bộ trạng thái hiện tại của điều hòa lên Cloud.

### `void app_rainmaker_update_custom_brands(char **brands, size_t count)`
Cập nhật danh sách các bộ mã lệnh IR đã học được để hiển thị trên menu chọn của ứng dụng di động.

## 6. Luồng dữ liệu (Data Flow)
1. **Cloud to Device:** Người dùng thao tác trên App -> RainMaker Cloud -> MQTT -> ESP32 Callback -> Gọi hàm `mgr_*_set_state` -> Phát lệnh IR.
2. **Device to Cloud:** Thay đổi cục bộ (nút bấm, Web UI) -> Gọi hàm `app_rainmaker_update_*` -> MQTT -> Cloud -> Cập nhật UI trên App.
