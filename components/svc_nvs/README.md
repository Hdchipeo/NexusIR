# Component: NVS Wrapper Service (svc_nvs)

## 1. Tổng quan (Overview)
Component này là một lớp bọc (wrapper) cho **Non-Volatile Storage (NVS)** của ESP-IDF. Nó cung cấp các API cấp cao để lưu trữ và quản lý các dữ liệu quan trọng cần được giữ lại sau khi mất điện, đặc biệt là các mã lệnh hồng ngoại (IR) đã học và các cấu hình thiết bị.

## 2. Tính năng (Features)
- Lưu trữ/Tải mã lệnh IR theo Key.
- Quản lý danh sách các Key đã lưu (sử dụng cJSON).
- Lưu trữ/Tải danh sách các thương hiệu tùy chỉnh (Custom Brands).
- Hỗ trợ đổi tên (Rename) hoặc xóa (Delete) mã lệnh.

## 3. Phụ thuộc (Dependencies)
- `nvs_flash`: Driver NVS cơ bản.
- `cJSON`: Để định dạng danh sách Key dưới dạng JSON.

## 4. Hướng dẫn sử dụng (Usage Example)

```c
#include "svc_nvs.h"

void app_main(void) {
    svc_nvs_init();

    // Lưu mã lệnh IR
    uint16_t raw_data[] = {9000, 4500, 560, 560...};
    svc_nvs_save_ir("ac_on_cmd", raw_data, sizeof(raw_data));

    // Lấy danh sách các lệnh đã lưu
    cJSON *keys = svc_nvs_get_ir_keys();
    char *json_str = cJSON_Print(keys);
    printf("Saved keys: %s\n", json_str);
    free(json_str);
    cJSON_Delete(keys);
}
```

## 5. API chính (Main API)

### `esp_err_t svc_nvs_init(void)`
Khởi tạo phân vùng NVS. Nếu phân vùng bị lỗi (do thay đổi cấu trúc), nó sẽ tự động format lại.

### `esp_err_t svc_nvs_save_ir(const char *key, const void *data, size_t len)`
Lưu mảng dữ liệu xung IR vào NVS với một định danh duy nhất.

### `cJSON *svc_nvs_get_ir_keys(void)`
Trả về một mảng JSON chứa tất cả các tên (key) mã lệnh IR đang có trong bộ nhớ.

### `esp_err_t svc_nvs_load_custom_brands(cJSON **brands_array)`
Tải danh sách các bộ mã lệnh (thương hiệu) mà người dùng đã tự tạo.
