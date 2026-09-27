# Component: Relay Manager (mgr_relay)

## 1. Tổng quan (Overview)
Component này quản lý 2 rơ-le (Relay) và 2 nút nhấn cảm ứng (Touch Button) tương ứng. Nó cho phép điều khiển rơ-le qua phần mềm hoặc trực tiếp bằng nút nhấn vật lý trên thiết bị, đồng thời hỗ trợ cơ chế đồng bộ trạng thái (Bridge) ra bên ngoài.

## 2. Phần cứng (Hardware Setup)
Mặc định chân GPIO (có thể đổi trong `menuconfig`):

| Chức năng | Chân GPIO mặc định | Ghi chú                          |
|-----------|--------------------|----------------------------------|
| Relay 1   | `GPIO 12`          | Đầu ra điều khiển Relay 1        |
| Relay 2   | `GPIO 13`          | Đầu ra điều khiển Relay 2        |
| Touch 1   | `GPIO 14`          | Đầu vào nút nhấn cảm ứng 1       |
| Touch 2   | `GPIO 15`          | Đầu vào nút nhấn cảm ứng 2       |

## 3. Phụ thuộc (Dependencies)
- `driver/gpio.h`: Driver GPIO cơ bản của ESP-IDF.
- `esp_timer`: Để xử lý chống rung hoặc các tác vụ trễ.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "mgr_relay.h"

void app_main(void) {
    // 1. Khởi tạo GPIO cho relay và nút nhấn
    mgr_relay_init();

    // 2. Điều khiển Relay 1 Bật
    mgr_relay_set_state(0, true, true);

    // 3. Đọc trạng thái Relay 2
    bool is_on = mgr_relay_get_state(1);
}
```

## 5. API chính (Main API)

### `esp_err_t mgr_relay_init(void)`
Cấu hình các chân GPIO làm Output (cho Relay) và Input (cho Touch Buttons). Khởi tạo trạng thái mặc định (thường là OFF).

### `void mgr_relay_set_state(int relay_idx, bool state, bool report_to_homekit)`
- `relay_idx`: 0 hoặc 1.
- `state`: `true` (ON) hoặc `false` (OFF).
- `report_to_homekit`: Nếu true, sẽ kích hoạt callback để cập nhật lên ứng dụng Nhà (Apple Home) hoặc Cloud.

### `void mgr_relay_set_bridge_cb(mgr_relay_bridge_cb_t cb)`
Đăng ký callback. Khi người dùng nhấn nút vật lý làm thay đổi trạng thái rơ-le, callback này sẽ được gọi để báo về `main` hoặc các tích hợp khác.

## 6. Logic hoạt động (Flow)
- Khi nút `Touch 1` được nhấn: Relay 1 sẽ đảo trạng thái (Toggle).
- Khi nút `Touch 2` được nhấn: Relay 2 sẽ đảo trạng thái (Toggle).
- Trạng thái được lưu vào NVS (nếu có cấu hình) để khôi phục sau khi mất điện.
