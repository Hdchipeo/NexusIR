# Component: IR Protocol Manager (mgr_ir_protocols)

## 1. Tổng quan (Overview)
Đây là component quan trọng nhất trong việc xử lý tín hiệu hồng ngoại ở tầng ứng dụng. Nó quản lý việc học lệnh (Learning), lưu trữ mã lệnh vào bộ nhớ (NVS/SPIFFS) theo cấu trúc Matrix (ma trận lệnh cho AC/Fan), và thực hiện phát các chuỗi xung hồng ngoại phức tạp.

## 2. Phần cứng (Hardware Setup)
Phụ thuộc trực tiếp vào `drv_ir_rmt` (GPIO 4 cho TX, GPIO 7 cho RX).

## 3. Phụ thuộc (Dependencies)
- `drv_ir_rmt`: Driver HAL cho RMT.
- `nvs_flash` & `spiffs`: Để lưu trữ dữ liệu bền vững.
- `ir_types.h`: Định nghĩa cấu trúc giao thức.

## 4. Hướng dẫn sử dụng (Usage Example)

### Học lệnh IR (Learning Mode)
```c
#include "mgr_ir_protocols.h"

void learn_flow() {
    mgr_ir_start_learn();
    // Chờ người dùng bấm remote vào mắt thu...
    
    uint32_t count;
    if (mgr_ir_get_learn_status(&count)) {
        // Đang học, đã nhận được 'count' xung
    }
    
    // Lưu vào matrix cho thiết bị "my_ac" tại vị trí index 1 (ví dụ 16 độ Cool)
    mgr_ir_save_to_matrix("my_ac", 1);
    mgr_ir_stop_learn();
}
```

### Phát lệnh từ Matrix
```c
// Phát lệnh số 5 của thiết bị "fan_living_room"
mgr_ir_send_from_matrix("fan_living_room", 5);
```

## 5. API chính (Main API)

### `esp_err_t mgr_ir_start_learn(void)`
Kích hoạt chế độ lắng nghe tín hiệu IR. Khi có tín hiệu, nó sẽ được lưu vào buffer tạm.

### `esp_err_t mgr_ir_save_to_matrix(const char *dev_id, int index)`
Lưu tín hiệu vừa học được vào một tệp tin trên SPIFFS. `dev_id` thường là tên thiết bị, `index` là mã định danh nút bấm hoặc trạng thái.

### `esp_err_t mgr_ir_send_from_matrix(const char *dev_id, int index)`
Đọc dữ liệu xung từ SPIFFS và phát ra qua LED hồng ngoại.

### `esp_err_t mgr_ir_send_raw(const uint16_t *durations, size_t count)`
Phát một chuỗi xung hồng ngoại bất kỳ (tính theo micro giây). Tương thích với định dạng Raw của thư viện IRremoteESP8266.

## 6. Cấu trúc lưu trữ (Storage Logic)
Mã lệnh được lưu trong thư mục `/spiffs/ir_matrix/`. Mỗi thiết bị có một file riêng chứa mảng các chuỗi xung, giúp việc truy xuất cực nhanh theo index mà không cần parse JSON hay XML phức tạp.
