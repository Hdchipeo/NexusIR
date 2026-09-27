# Component: IR Protocol Manager & Core (mgr_ir_protocols)

## 1. Tổng quan Kiến trúc (Architecture Overview)

Component `mgr_ir_protocols` là lõi xử lý hồng ngoại toàn diện thế hệ mới (IR Core V3) của NexusIR trên nền tảng ESP32-C3 (ESP-IDF v5.5.4). Hệ thống được thiết kế theo kiến trúc hướng sự kiện (Event-Driven), phân lớp chặt chẽ và an toàn 24/7:

```
+-----------------------------------------------------------------------------------+
|               Tầng Ứng dụng & Dịch vụ (Web, HomeKit, RainMaker, ESP-NOW)          |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|     mgr_ir_protocols: Session State Machine, Worker Task, Normalization & CRC     |
+-----------------------------------------------------------------------------------+
       | (HAL APIs)                                       | (Storage APIs)
       v                                                  v
+-----------------------------+           +-----------------------------------------+
|  drv_ir_rmt (Hardware HAL)  |           |        svc_nvs (NVS Namespace: ir_data) |
|  - RMT TX Channel & Carrier |           |  - V1 Packed Header (20B) + CRC32       |
|  - RMT RX Channel & ISR CB  |           |  - Dual Migration Engine (Safe/Atomic)  |
+-----------------------------+           +-----------------------------------------+
```

---

## 2. Phần cứng & Chân kết nối (Hardware Pinout & HAL)

Hệ thống phân tách triệt để giữa tầng logic (`mgr_ir_protocols`) và tầng điều khiển phần cứng (`drv_ir_rmt`):
- **TX Pin:** GPIO 4 (Nối tầng lái Transistor / MOSFET phát hồng ngoại).
- **RX Pin:** GPIO 7 (Nối mắt thu giải điều chế IR Demodulator 38kHz tích hợp mạch lọc nhiễu quang).
- **RMT Clock Resolution:** 1,000,000 Hz (độ phân giải 1 microsecond/tick).
- **RMT Symbols Memory:** 64 symbols memory block trên mỗi kênh.
- **Tính tương thích chip:** Nhận biết cấu hình DMA trên ESP32-S3 và tối ưu không DMA trên ESP32-C3.

---

## 3. Định dạng Bản ghi V1 & Kiểm tra Toàn vẹn CRC32 (Wire Format V1)

Dữ liệu IR được lưu trữ bền vững trong NVS dưới định dạng nhị phân đóng gói chặt chẽ `#pragma pack(push, 1)`, header cố định đúng 20 bytes:

```
+-----------------------------------------------------------------------+
| Magic (4B)  | Ver (1B) | Flags (1B) | Carrier (2B) | Duty (1B) | Rep (1B) |
| 0x5249584E  |   0x01   | Bitmask    |  38000 Hz    |    33%    |  0 or 1  |
+-----------------------------------------------------------------------+
| Gap (2B)    | DurCount (2B) |       CRC32 (4B, Little-Endian)         |
| 40 ms       | N halfwords   | esp_rom_crc32_le over raw pulse payload |
+-----------------------------------------------------------------------+
| Payload: uint16_t durations[DurCount]                                |
| [Mark 0, Space 0, Mark 1, Space 1, ... Mark N-1]                      |
+-----------------------------------------------------------------------+
```

### Chi tiết các trường:
- **Magic:** `0x5249584E` (`"NXIR"` - NexusIR).
- **Version:** `1` (IR_RECORD_VERSION_V1).
- **Flags:**
  - `IR_FLAG_TRUNCATED` (0x01): Báo hiệu tín hiệu chạm ngưỡng đệm tối đa (buffer full).
  - `IR_FLAG_CARRIER_CUSTOM` (0x02): Tần số carrier khác mặc định.
  - `IR_FLAG_FAN_REPEAT` (0x04): Thiết bị yêu cầu phát lặp (ví dụ Quạt).
- **Carrier Freq:** Tần số sóng mang (10,000 - 60,000 Hz, mặc định 38000 Hz).
- **Duty Cycle:** Độ rộng xung sóng mang (mặc định 33%).
- **Repeat Count:** Số lần lặp lại sau lần phát đầu (0 = phát 1 lần, 1 = phát 2 lần).
- **Repeat Gap:** Thời gian nghỉ giữa các lần phát lặp (mặc định 40 ms).
- **Duration Count:** Số phần tử `uint16_t` trong payload.
- **CRC32:** Mã checksum IEEE 802.3 tính bằng `esp_rom_crc32_le()` phần cứng trên toàn bộ payload.

---

## 4. Ngân sách Lưu trữ NVS & Bảng Dung lượng (IR-12 Storage Budget)

Hệ thống phân vùng Flash của NexusIR dành riêng 32 KB cho `nvs`:
- **Tổng kích thước phân vùng NVS:** 0x8000 (32,768 bytes = 8 Flash Pages).
- **NVS Overhead:** Mỗi page tiêu tốn 32 bytes header + 128 bytes bitmap. 1 page dành cho Garbage Collection.
- **Dung lượng khả dụng thực tế:** ~27.5 KB.

| Loại lệnh IR | Số xung trung bình | Kích thước Payload | Tổng kích thước V1 (Header + Payload) | Số Entry NVS chiếm dụng | Dung lượng ước tính trong NVS |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **TV / Quạt (NEC/RC5)** | 32 - 68 xung | 64 - 136 bytes | **84 - 156 bytes** | ~3 - 5 entries | **180 - 220 phím** |
| **Điều hòa (AC Matrix)** | 100 - 240 xung | 200 - 480 bytes | **220 - 500 bytes** | ~7 - 16 entries | **35 - 45 phím AC** |

### Đánh giá & Quyết định Kiến trúc:
- Một profile AC hoàn chỉnh gồm 16 trạng thái (Off, 16°C – 30°C) chiếm khoảng **5.6 KB**.
- Kết hợp với 20 phím TV/Quạt (~2.5 KB), tổng dung lượng chỉ chiếm **~8.1 KB / 27.5 KB (~30% NVS)**.
- **Kết luận:** NVS hoàn toàn đáp ứng xuất sắc nhu cầu lưu trữ tốc độ cao, truy xuất nguyên tử (atomic write) mà không cần phụ thuộc LittleFS/SPIFFS.
- **Cơ chế phòng vệ:** Khi NVS báo `ESP_ERR_NVS_NOT_ENOUGH_SPACE`, hệ thống từ chối lưu lệnh mới và trả về HTTP 507, đảm bảo toàn vẹn các lệnh cũ đã lưu.

---

## 5. Kiến trúc Hướng sự kiện & Quản lý Phiên (Event-Driven State Machine)

- **ISR Siêu tinh gọn (Ultra-thin ISR):** RMT RX ISR chỉ đóng gói `ir_event_t` và đẩy vào FreeRTOS Queue `s_ir_evt_queue` qua `xQueueSendFromISR` (< 1.5 µs).
- **Worker Task Riêng biệt (`mgr_ir_worker_task`):** Chạy ở Priority 5, Stack 4096 bytes. Thực hiện re-arming, lọc nhiễu (< 20 symbols), điều khiển LED phản hồi và ngắt thu khi có tín hiệu hợp lệ.
- **Session ID:** Mỗi lần kích hoạt học lệnh, `s_current_session_id` tự động tăng đơn điệu. Mọi sự kiện trễ (stale events) từ phiên cũ bị loại bỏ ngay lập tức.
- **Hardware Watchdog Timer:** Tự động hủy học sau 15 giây (`DEFAULT_LEARN_TIMEOUT_MS`) nếu không có remote bấm, chuyển trạng thái sang `IR_STATE_TIMEOUT` và giải phóng RMT RX.

---

## 6. Cơ chế Di trú An toàn Đa Tầng (Safe Migration Engine)

Tương thích ngược tuyệt đối với toàn bộ dữ liệu NVS cũ của người dùng:
1. **On-Access Migration:** Khi người dùng bấm phát bất kỳ phím nào qua Web/HomeKit/RainMaker, hàm `mgr_ir_send_key()` kiểm tra định dạng. Nếu phát hiện lệnh cũ (Legacy 0xAA RAW hoặc Legacy Matrix array), nó tự động nâng cấp sang V1 kèm CRC32 và lưu lại vào NVS.
2. **Background Batch Migration:** Lúc khởi động, `mgr_ir_worker_task` quét toàn bộ namespace `ir_data` bằng `nvs_entry_find()`, xác thực CRC32 các key V1 và tự động chuyển đổi các key legacy.
3. **An toàn khi mất điện (Power-loss safe):** Hệ thống chỉ ghi đè bản ghi mới khi chuỗi xung V1 đã serialize và validate thành công. Không bao giờ xóa key cũ trước khi key mới sẵn sàng.

---

## 7. Bộ API Dịch vụ & REST Endpoints (Unified APIs)

### C API:
- `mgr_ir_init()`: Khởi tạo IR Core, HAL và Worker Task.
- `mgr_ir_start_learn()` / `mgr_ir_start_learn_with_timeout(timeout_ms, out_session_id)`: Bắt đầu học lệnh.
- `mgr_ir_stop_learn()` / `mgr_ir_cancel_learn()`: Hủy phiên học.
- `mgr_ir_get_session_info(out_info)`: Lấy thông tin trạng thái, session ID, số xung và cờ truncated.
- `mgr_ir_send_key(key)`: Phát phím lưu trong NVS (tự động cấu hình carrier và repeat).
- `mgr_ir_send_from_matrix(dev_id, index)`: Phát lệnh AC matrix từ NVS.
- `mgr_ir_save_learned_result(key)`: Lưu xung vừa học dưới dạng V1 có CRC32.
- `mgr_ir_export_key_json(key, out_json)`: Xuất key ra chuỗi JSON.
- `mgr_ir_import_key_json(key, json_str)`: Nhập và validate chuỗi JSON rồi lưu vào NVS.
- `mgr_ir_migrate_all(migrated, skipped, errors)`: Quét và nâng cấp toàn bộ phân vùng NVS.

### HTTP REST APIs:
- `GET /api/learn/status`: Trả về trạng thái học lệnh (`status`, `session_id`, `captured`, `truncated`).
- `POST /api/send?key=KEY`: Phát lệnh IR. Trả về:
  - `200 OK`: Phát thành công.
  - `409 Conflict`: Thiết bị đang bận học lệnh (ARMED), từ chối phát để bảo vệ phần cứng.
  - `404 Not Found`: Không tìm thấy key trong NVS.
  - `500 Internal Error`: Lỗi giải mã hoặc phần cứng.
- `GET /api/ir/export?key=KEY`: Xuất key ra file JSON kèm metadata và mảng `durations`.
- `POST /api/ir/import?key=KEY`: Nhập key từ JSON (kiểm tra độ dài, giới hạn 10 - 1200 pulses).
- `POST /api/ir/migrate`: Chạy quét di trú NVS theo yêu cầu.
- `GET /api/ir/storage`: Lấy số liệu phân vùng NVS (`used_entries`, `free_entries`, dung lượng ước tính).

---

## 8. Hướng dẫn Kiểm thử Hồi quy trên Máy Host (Host Unit Testing)

Component cung cấp bộ test suite độc lập chạy trực tiếp trên macOS/Linux mà không cần nạp vào ESP32:

```bash
# Biên dịch và chạy bộ test host
clang -O2 -Wall -Wextra -Icomponents/mgr_ir_protocols/include test/test_ir_waveform_host.c -o test/test_ir_waveform_host
./test/test_ir_waveform_host
```

**Các ca kiểm thử tự động bao gồm:**
1. `test_waveform_normalization`: Kiểm tra loại bỏ leading space, lọc 0-duration, cắt trailing zero.
2. `test_odd_duration_overread_prevention`: Kiểm tra ngăn chặn lỗi đọc tràn bộ nhớ với mảng durations số lẻ.
3. `test_v1_serialization_and_deserialization`: Kiểm tra đóng gói Header V1, endianness và giải mã.
4. `test_crc32_corruption_detection`: Kiểm tra phát hiện hỏng dữ liệu khi payload bị thay đổi 1 bit.
5. `test_legacy_raw_upgrade`: Kiểm tra nâng cấp tự động bản ghi Legacy 0xAA RAW -> V1.
6. `test_legacy_matrix_upgrade`: Kiểm tra nâng cấp tự động mảng Legacy Matrix -> V1.
