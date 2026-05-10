# Component: IR RMT Driver (drv_ir_rmt)

## 1. Tổng quan (Overview)
Component này là lớp trừu tượng phần cứng (HAL) để điều khiển ngoại vi RMT (Remote Control Peripheral) của ESP32. Nhiệm vụ chính là khởi tạo và truyền các xung hồng ngoại (IR) thô hoặc nhận tín hiệu IR từ mắt thu.

## 2. Phần cứng (Hardware Setup)
Mặc định, component sử dụng các chân GPIO sau (cấu hình trong `menuconfig` -> `Device Configuration` -> `IR Hardware`):

| Chức năng      | Chân GPIO mặc định | Ghi chú                          |
|----------------|--------------------|----------------------------------|
| IR TX Pin      | `GPIO 4`           | Nối với chân Data của LED hồng ngoại qua transistor |
| IR RX Pin      | `GPIO 7`           | Nối với chân Out của mắt thu hồng ngoại (e.g. TSOP38238) |

## 3. Phụ thuộc (Dependencies)
- `driver/rmt.h`: Driver RMT của ESP-IDF.
- `ir_types.h`: Chứa các định nghĩa struct và enum dùng chung cho IR.

## 4. Cấu hình (Configuration)
Cấu hình qua `idf.py menuconfig`:
- `CONFIG_APP_IR_TX_GPIO`: Chân phát IR.
- `CONFIG_APP_IR_RX_GPIO`: Chân thu IR.

## 5. Hướng dẫn sử dụng (Usage Example)

```c
#include "drv_ir_rmt.h"

void app_main(void) {
    // 1. Cấu hình engine
    ir_engine_config_t config = {
        .gpio_num = 4,
        .resolution_hz = 1000000 // 1MHz = 1us resolution
    };
    ir_engine_init(&config);

    // 2. Gửi dữ liệu raw (rmt_symbol_word_t)
    // rmt_symbol_word_t items[] = { ... };
    // ir_engine_send_raw(items, count);
}
```

## 6. API chính (Main API)

### `esp_err_t ir_engine_init(const ir_engine_config_t *config)`
Khởi tạo ngoại vi RMT cho mục đích phát/thu IR.
- **Param:** `config` - Chứa GPIO và độ phân giải clock.
- **Return:** `ESP_OK` nếu thành công.

### `esp_err_t ir_engine_send_raw(const void *symbols, size_t count)`
Gửi mảng các ký hiệu RMT ra chân TX. Hàm này sẽ đợi cho đến khi việc truyền hoàn tất (blocking).
- **Param:** `symbols` - Con trỏ tới mảng `rmt_symbol_word_t`.
- **Param:** `count` - Số lượng phần tử trong mảng.
