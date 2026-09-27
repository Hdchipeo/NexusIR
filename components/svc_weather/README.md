# Component: Weather Service (svc_weather)

## 1. Tổng quan (Overview)
Component này chịu trách nhiệm lấy dữ liệu thời tiết thực tế từ Internet (thông qua OpenWeatherMap hoặc các API tương đương). Dữ liệu này sau đó được sử dụng để hiển thị lên màn hình LCD của thiết bị.

## 2. Tính năng (Features)
- Gửi yêu cầu HTTP GET tới Weather API.
- Phân giải (Parse) dữ liệu JSON để lấy nhiệt độ, độ ẩm và trạng thái thời tiết (Nắng, Mưa, Mây...).
- Cập nhật định kỳ dữ liệu để đảm bảo thông tin hiển thị luôn mới.

## 3. Phụ thuộc (Dependencies)
- `esp_http_client`: Để thực hiện các yêu cầu mạng.
- `cJSON`: Để xử lý dữ liệu JSON trả về từ API.
- `mgr_display`: Để đẩy dữ liệu thời tiết lên UI.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "svc_weather.h"

void app_main(void) {
    // Khởi tạo và bắt đầu cập nhật thời tiết trong background
    svc_weather_init();
}
```

## 5. API chính (Main API)

### `esp_err_t svc_weather_init(void)`
Khởi tạo service, bắt đầu task định kỳ thực hiện các truy vấn HTTP tới server thời tiết.

## 6. Lưu ý
- Thiết bị cần có kết nối WiFi để service này hoạt động.
- API Key và địa điểm (City/Lat-Lon) thường được cấu hình cứng trong code hoặc qua `menuconfig`.
- Nếu không có mạng, service sẽ thử lại sau một khoảng thời gian chờ.
