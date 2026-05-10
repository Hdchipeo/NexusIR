# Component: Fan Logic Manager (mgr_fan_logic)

## 1. Tổng quan (Overview)
Component này quản lý trạng thái và logic điều khiển Quạt (Fan). Nó xử lý các thuộc tính như: Nguồn (Power), Tốc độ (Speed - discrete 1, 2, 3) và Đảo gió (Swing). Tương tự AC Manager, nó là cầu nối giữa các service điều khiển từ xa và việc phát lệnh IR.

## 2. Phần cứng (Hardware Setup)
Phụ thuộc vào `drv_ir_rmt` để phát tín hiệu hồng ngoại điều khiển quạt vật lý.

## 3. Phụ thuộc (Dependencies)
- `drv_ir_rmt`: Driver phát IR.
- `mgr_ir_protocols`: Hệ thống quản lý mã lệnh (Matrix).
- `ir_types.h`: Định nghĩa struct `ir_fan_state_t`.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "mgr_fan_logic.h"

void app_main(void) {
    // 1. Khởi tạo
    mgr_fan_init();

    // 2. Thiết lập trạng thái
    ir_fan_state_t fan_state = {
        .power = true,
        .speed = 2, // Medium
        .swing = false
    };
    mgr_fan_set_state(&fan_state);

    // 3. Thực thi gửi lệnh IR
    mgr_fan_send();
}
```

## 5. API chính (Main API)

### `void mgr_fan_init(void)`
Khởi tạo và khôi phục trạng thái quạt từ NVS.

### `void mgr_fan_set_state(const ir_fan_state_t *state)`
Cập nhật trạng thái quạt vào bộ nhớ quản lý.

### `esp_err_t mgr_fan_send(void)`
Dựa trên trạng thái hiện tại, tìm mã IR tương ứng trong Matrix và phát ra LED hồng ngoại.

### `void mgr_fan_set_custom_brand(const char *name)`
Thiết lập tên bộ mã lệnh tùy chỉnh cho quạt (dùng khi quạt được "học" lệnh thủ công).
