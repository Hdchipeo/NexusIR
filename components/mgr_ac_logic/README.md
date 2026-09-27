# Component: AC Logic Manager (mgr_ac_logic)

## 1. Tổng quan (Overview)
Component này quản lý logic trạng thái của Điều hòa (AC). Nó lưu giữ các thông tin như: nguồn (Power), nhiệt độ (Temperature), chế độ (Mode), tốc độ quạt (Fan speed) và hướng gió (Swing). Nó đóng vai trò trung gian giữa tầng dịch vụ (HomeKit/RainMaker) và tầng driver hồng ngoại (IR).

## 2. Phần cứng (Hardware Setup)
Component này không trực tiếp điều khiển phần cứng mà phụ thuộc vào `drv_ir_rmt` để phát tín hiệu. Tuy nhiên, logic của nó sẽ quyết định mã lệnh IR nào được phát ra.

## 3. Phụ thuộc (Dependencies)
- `drv_ir_rmt`: Để phát tín hiệu IR.
- `mgr_ir_protocols`: Để lấy dữ liệu mã lệnh từ Matrix/NVS.
- `ir_types.h`: Định nghĩa các kiểu dữ liệu `ir_ac_state_t`.

## 4. Cấu hình (Configuration)
Chủ yếu cấu hình qua `menuconfig` -> `Device Configuration` -> `AC (Air Conditioner)`:
- `APP_ESPNOW_AC_MODE`: Chế độ hoạt động (Master/Slave/Standalone).

## 5. Hướng dẫn sử dụng (Usage Example)

```c
#include "mgr_ac_logic.h"

void app_main(void) {
    // 1. Khởi tạo (load trạng thái cũ từ NVS)
    mgr_ac_init();

    // 2. Thay đổi trạng thái
    ir_ac_state_t state;
    mgr_ac_get_state(&state);
    state.power = true;
    state.temp = 24;
    state.mode = IR_AC_MODE_COOL;
    mgr_ac_set_state(&state);

    // 3. Phát lệnh IR thực tế
    mgr_ac_send();
}
```

## 6. API chính (Main API)

### `void mgr_ac_init(void)`
Khởi tạo module AC, khôi phục trạng thái cuối cùng và cấu hình thương hiệu (brand) từ bộ nhớ NVS.

### `void mgr_ac_set_state(const ir_ac_state_t *state)`
Cập nhật trạng thái AC trong bộ nhớ. Hàm này chưa phát lệnh IR.

### `esp_err_t mgr_ac_send(void)`
Thực hiện phát lệnh IR dựa trên trạng thái và thương hiệu hiện tại. Nó sẽ tự động tìm kiếm mã lệnh tương ứng trong hệ thống Matrix.

### `void mgr_ac_set_bridge_cb(mgr_ac_bridge_cb_t cb)`
Đăng ký hàm callback để đồng bộ trạng thái sang các service khác (như gửi gói tin ESP-NOW khi trạng thái thay đổi).
