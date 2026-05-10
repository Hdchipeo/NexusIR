# Component: LED Strip Driver (drv_led_strip)

## 1. Tổng quan (Overview)
Component này quản lý dải LED RGB (thường là WS2812B) và các hiệu ứng ánh sáng. Nó hỗ trợ tối đa 5 bóng LED (hoặc 5 dải LED độc lập tùy cấu hình) và cung cấp một hệ thống trạng thái (System States) để hiển thị phản hồi trực quan cho người dùng (như đang cấu hình WiFi, đang gửi IR, lỗi...).

## 2. Phần cứng (Hardware Setup)
Hỗ trợ tối đa 5 đèn. Chân GPIO mặc định:

| Đèn       | Chân GPIO mặc định | Số lượng LED (mặc định) |
|-----------|--------------------|-------------------------|
| Lamp 1    | `GPIO 2`           | 8                       |
| Lamp 2    | `GPIO 8`           | 8                       |
| Lamp 3    | `GPIO 9`           | 8                       |
| Lamp 4    | `GPIO 10`          | 8                       |
| Lamp 5    | `GPIO 18`          | 8                       |

## 3. Phụ thuộc (Dependencies)
- `led_strip`: Thư viện driver LED Strip của ESP-IDF (dựa trên RMT hoặc SPI).
- `esp_timer`: Dùng để tạo các hiệu ứng chạy ngầm.

## 4. Cấu hình (Configuration)
Cấu hình chi tiết trong `menuconfig` -> `Device Configuration` -> `LED Lamp X`:
- `APP_LEDX_GPIO`: Chân dữ liệu.
- `APP_LEDX_COUNT`: Số lượng bóng trên dải.
- `APP_LEDX_TYPE`: Kiểu LED (Strip RGB hoặc Single LED PWM).

## 5. Hướng dẫn sử dụng (Usage Example)

```c
#include "drv_led.h"

void app_main(void) {
    // 1. Khởi tạo toàn bộ hệ thống LED
    drv_led_init();

    // 2. Đặt trạng thái hệ thống (ví dụ: đang truyền IR)
    drv_led_set_state(DRV_LED_IR_TX);

    // 3. Điều khiển thủ công từng đèn (Lamp ID 0-4)
    drv_led_set_color(0, 255, 0, 0); // Đèn 1 màu Đỏ
    drv_led_set_effect(0, DRV_LED_EFFECT_BREATHING); // Hiệu ứng nhịp thở
}
```

## 6. API chính (Main API)

### `esp_err_t drv_led_set_state(drv_led_state_t state)`
Cập nhật trạng thái hiển thị của toàn bộ hệ thống. Các trạng thái bao gồm: `STARTUP`, `WIFI_PROV`, `WIFI_CONN`, `IR_TX`, `IR_LEARN`, `IDLE`...

### `esp_err_t drv_led_set_effect(uint8_t lamp_id, drv_led_effect_t effect)`
Thiết lập hiệu ứng cho một đèn cụ thể. Các hiệu ứng: `STATIC`, `RAINBOW`, `BREATHING`, `BLINK`, `FIRE`, `SPARKLE`...

### `esp_err_t drv_led_set_color(uint8_t lamp_id, uint8_t r, uint8_t g, uint8_t b)`
Thiết lập màu sắc RGB cho đèn.

## 7. Trạng thái hệ thống (System States Flow)
Màu sắc mặc định theo trạng thái:
- **WIFI_PROV:** Nháy màu Xanh dương.
- **IR_TX:** Nháy nhanh màu Trắng/Xanh lá.
- **IR_FAIL:** Nháy Đỏ.
- **OTA:** Hiệu ứng Loading màu Vàng.
