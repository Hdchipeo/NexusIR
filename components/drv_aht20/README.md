# Component: AHT20 Sensor Driver (drv_aht20)

## 1. Tổng quan (Overview)
Component này cung cấp API để giao tiếp với cảm biến nhiệt độ và độ ẩm AHT20 qua giao tiếp I2C. Nó xử lý việc khởi tạo bus I2C và đọc dữ liệu thô từ cảm biến, sau đó chuyển đổi sang độ C và % độ ẩm.

## 2. Phần cứng (Hardware Setup)
Cần kết nối cảm biến AHT20 vào bus I2C của ESP32:

| Chức năng | Chân GPIO mặc định | Ghi chú                          |
|-----------|--------------------|----------------------------------|
| I2C SDA   | `GPIO 6`           | Chân dữ liệu I2C                 |
| I2C SCL   | `GPIO 5`           | Chân xung clock I2C              |

*Lưu ý: Chân GPIO có thể thay đổi trong `menuconfig` -> `Device Configuration` -> `Temperature & Humidity Sensor`.*

## 3. Phụ thuộc (Dependencies)
- `i2c_bus`: Component quản lý bus I2C (thường là managed component của Espressif).
- `esp_err`: Header định nghĩa các mã lỗi của ESP-IDF.

## 4. Cấu hình (Configuration)
Cấu hình qua `idf.py menuconfig`:
- `CONFIG_APP_I2C_SDA`: GPIO cho SDA.
- `CONFIG_APP_I2C_SCL`: GPIO cho SCL.

## 5. Hướng dẫn sử dụng (Usage Example)

```c
#include "drv_aht20.h"

void app_main(void) {
    // 1. Khởi tạo cảm biến
    if (drv_aht20_init() == ESP_OK) {
        float temperature, humidity;
        
        // 2. Đọc dữ liệu định kỳ
        if (aht20_sensor_read(&temperature, &humidity) == ESP_OK) {
            printf("Temp: %.2f C, Hum: %.2f %%\n", temperature, humidity);
        }
    }
}
```

## 6. API chính (Main API)

### `esp_err_t drv_aht20_init(void)`
Khởi tạo bus I2C và kiểm tra sự hiện diện của cảm biến AHT20.
- **Return:** `ESP_OK` nếu khởi tạo thành công và cảm biến phản hồi.

### `esp_err_t aht20_sensor_read(float *temp, float *hum)`
Kích hoạt đo lường và đọc giá trị nhiệt độ, độ ẩm.
- **Param:** `temp` - Con trỏ lưu giá trị nhiệt độ (Celsius).
- **Param:** `hum` - Con trỏ lưu giá trị độ ẩm (%).
- **Return:** `ESP_OK` nếu đọc dữ liệu hợp lệ.
