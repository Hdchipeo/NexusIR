# Component: System Utilities (sys_utils)

## 1. Tổng quan (Overview)
Component này cung cấp các công cụ tiện ích hệ thống, chủ yếu tập trung vào việc giám sát tài nguyên bộ nhớ (Heap/PSRAM). Nó giúp các component khác kiểm tra xem hệ thống còn đủ bộ nhớ để thực hiện các tác vụ nặng (như parse JSON lớn hoặc nạp ảnh) hay không, nhằm tránh lỗi crash hệ thống do hết bộ nhớ.

## 2. Tính năng (Features)
- Theo dõi dung lượng bộ nhớ Heap còn trống trong thời gian thực.
- Phân biệt giữa bộ nhớ nội (Internal RAM) và bộ nhớ ngoài (PSRAM).
- Cung cấp cơ chế "Safety Check" trước khi cấp phát bộ nhớ động.

## 3. Phụ thuộc (Dependencies)
- `esp_system`: API lõi của ESP-IDF.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "sys_mem.h"

void app_main(void) {
    // 1. Khởi tạo giám sát
    sys_mem_init();

    // 2. Kiểm tra an toàn trước khi xử lý file lớn
    size_t required = 10 * 1024; // 10KB
    if (sys_mem_is_safe(required, false)) {
        char *buffer = malloc(required);
        // ... process ...
    } else {
        ESP_LOGE("APP", "Low memory! Skipping operation.");
    }
}
```

## 5. API chính (Main API)

### `size_t sys_mem_get_free_internal(void)`
Trả về dung lượng bộ nhớ RAM nội (tốc độ cao) còn trống tính theo byte.

### `size_t sys_mem_get_free_psram(void)`
Trả về dung lượng bộ nhớ PSRAM còn trống (nếu board có hỗ trợ).

### `bool sys_mem_is_safe(size_t size, bool use_psram)`
Kiểm tra xem hệ thống có đủ `size` byte bộ nhớ hay không, có tính toán đến một ngưỡng an toàn để tránh cạn kiệt bộ nhớ hoàn toàn.
