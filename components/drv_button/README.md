# Component: Button Driver (drv_button)

## 1. Tổng quan (Overview)
Component này quản lý các nút nhấn vật lý trên thiết bị. Nó sử dụng thư viện `iot_button` để xử lý chống rung (debounce) và các sự kiện nhấn (click, long press). Trong hệ thống NexusIR, nút nhấn chủ yếu dùng để đánh thức màn hình và thực hiện khôi phục cài đặt gốc (Factory Reset).

## 2. Phần cứng (Hardware Setup)
Kết nối nút nhấn vật lý:

| Chức năng | Chân GPIO mặc định | Ghi chú                          |
|-----------|--------------------|----------------------------------|
| Button    | `GPIO 3`           | Thường là nút BOOT trên board ESP32 |

*Lưu ý: Mức tích cực (Active Level) mặc định là `0` (Low - nhấn nút nối xuống GND).*

## 3. Phụ thuộc (Dependencies)
- `espressif/button`: Thư viện `iot_button` từ ESP Component Registry.
- `mgr_display`: Để gọi hàm đánh thức màn hình khi nhấn nút.
- `drv_led`: Để hiển thị trạng thái khi Factory Reset.
- `nvs_flash`: Để xóa dữ liệu khi Factory Reset.

## 4. Cấu hình (Configuration)
Cấu hình qua `idf.py menuconfig` -> `Device Configuration`:
- `CONFIG_APP_BUTTON_GPIO`: Chọn chân GPIO cho nút nhấn.
- `CONFIG_APP_BUTTON_ACTIVE_LEVEL`: Mức logic khi nhấn nút (0 hoặc 1).

## 5. Hướng dẫn sử dụng (Usage Example)

```c
#include "drv_button.h"

void app_main(void) {
    // Chỉ cần gọi init, các callback đã được đăng ký bên trong
    drv_button_init();
}
```

## 6. Logic hoạt động (Behavior)
- **Nhấn bất kỳ (Down):** Gọi `mgr_display_activity_tick()` để bật sáng màn hình hoặc giữ màn hình không bị tắt.
- **Nhấn giữ (3 giây):** 
    1. Đổi màu LED sang chế độ `DRV_LED_IR_LEARN` (chỉ báo trực quan).
    2. Xóa toàn bộ phân vùng NVS (`nvs_flash_erase`).
    3. Khởi động lại chip (`esp_restart`).
