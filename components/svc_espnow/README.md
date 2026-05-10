# Component: ESP-NOW Service (svc_espnow)

## 1. Tổng quan (Overview)
Component này quản lý việc truyền nhận dữ liệu không dây giữa các thiết bị NexusIR thông qua giao thức **ESP-NOW**. Đây là giải pháp kết nối năng lượng thấp, độ trễ cực thấp, không phụ thuộc vào bộ định tuyến WiFi (Router), cho phép các thiết bị "nói chuyện" trực tiếp với nhau.

## 2. Các loại dữ liệu hỗ trợ (Message Types)
- **AC State:** Đồng bộ trạng thái điều hòa giữa Master và Slave.
- **Fan State:** Đồng bộ trạng thái quạt.
- **LED Control:** Điều khiển dải LED từ xa.
- **Sensor Data:** Gửi dữ liệu nhiệt độ/độ ẩm từ cảm biến pin về hub trung tâm.
- **Relay Control:** Bật/Tắt relay từ xa.

## 3. Phụ thuộc (Dependencies)
- `esp_now`: Driver ESP-NOW chính thức của ESP-IDF.
- `ir_types.h`: Định nghĩa cấu trúc dữ liệu AC/Fan.

## 4. Hướng dẫn sử dụng (Usage Example)

### Gửi dữ liệu (Bridge Master)
```c
#include "svc_espnow.h"

void sync_ac() {
    ir_ac_state_t state = { ... };
    // Gửi trạng thái AC tới tất cả các Slave đã add peer
    svc_espnow_bridge_ac_send(&state, AC_BRAND_CUSTOM, "MyAC");
}
```

### Nhận dữ liệu (Slave Side)
```c
void on_ac_received(const ir_ac_state_t *state, ac_brand_t brand, const char *name) {
    // Thực hiện phát lệnh IR tương ứng
    mgr_ac_set_state(state);
    mgr_ac_send();
}

void app_main() {
    svc_espnow_init();
    svc_espnow_register_ac_handler(on_ac_received);
}
```

## 5. API chính (Main API)

### `esp_err_t svc_espnow_init(void)`
Khởi tạo ngăn xếp ESP-NOW, đăng ký callback nhận dữ liệu mặc định.

### `esp_err_t svc_espnow_add_peer(const uint8_t *mac)`
Thêm địa chỉ MAC của thiết bị đích vào danh sách liên kết.

### `esp_err_t svc_espnow_bridge_ac_send(...)`
Đóng gói trạng thái AC vào frame ESP-NOW và gửi đi. Tương tự cho các hàm `_led_send`, `_fan_send`, `_temp_send`, `_relay_send`.

## 6. Cơ chế hoạt động (Logic)
NexusIR sử dụng ESP-NOW để mở rộng hệ sinh thái. Một thiết bị có thể cấu hình là **Master** (nhận lệnh từ HomeKit/Web và phát đi) hoặc **Slave** (nhận lệnh từ ESP-NOW và thực thi phần cứng tại chỗ).
