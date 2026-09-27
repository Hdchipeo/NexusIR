# Component: IR RMT Driver (drv_ir_rmt)

## 1. Tổng quan (Overview)
Component `drv_ir_rmt` đóng vai trò là Lớp Trừu tượng Phần cứng (HAL) thuần túy cho ngoại vi RMT (Remote Control Peripheral) trên ESP-IDF 5.x. Driver chịu trách nhiệm sở hữu và quản lý toàn bộ vòng đời của kênh truyền TX và kênh nhận RX, đảm bảo hệ thống không bị crash hoặc panics khi phần cứng gặp sự cố trong môi trường vận hành 24/7.

## 2. Phần cứng (Hardware Setup)

| Chức năng | Chân GPIO mặc định | Đặc tả cấu hình |
| :--- | :--- | :--- |
| **IR TX Pin** | `GPIO 4` | Nối tầng lái Transistor / MOSFET phát LED IR. Resolution: 1 MHz. Mem block: 64 symbols. |
| **IR RX Pin** | `GPIO 7` | Nối mắt thu giải điều chế IR (e.g. TSOP38238). Cấu hình nội bộ PULLUP, Invert Level. |

## 3. Các tính năng cốt lõi (Core Features)

1. **Phân tách trách nhiệm hoàn toàn (Separation of Concerns):**
   - Tầng ứng dụng (`mgr_ir_protocols`) không bao giờ trực tiếp thao tác thanh ghi hoặc API cấp thấp của ESP-IDF RMT.
   - Toàn bộ kênh TX, RX, Encoder và ISR callback đều được đóng gói bên trong driver này.

2. **Cấu hình Sóng mang Động (Dynamic Carrier Frequency - IR-17):**
   - Hỗ trợ thay đổi tần số điều chế (10,000 Hz – 60,000 Hz, ví dụ 36kHz, 38kHz, 40kHz, 56kHz) và duty cycle runtime qua hàm `ir_engine_set_carrier()`.
   - Có cơ chế kiểm tra cache: Nếu tần số và duty cycle yêu cầu trùng với cấu hình hiện tại, driver bỏ qua việc re-apply để tránh overhead.

3. **An toàn khi Vận hành 24/7 (Fail-Safe Operation):**
   - **Loại bỏ hoàn toàn `ESP_ERROR_CHECK()`:** Mọi lỗi phần cứng đều được ghi log chi tiết và trả về mã `esp_err_t` thay vì làm reboot chip.
   - **Timeout an toàn 3 giây khi phát:** Thay thế chờ vô tận `-1` bằng `rmt_tx_wait_all_done(..., pdMS_TO_TICKS(3000))` để bảo vệ hệ thống không bị treo vĩnh viễn nếu ngoại vi RMT gặp sự cố.

## 4. API chính (Main HAL APIs)

### TX APIs
- `esp_err_t ir_engine_init(const ir_engine_config_t *config);`
  Khởi tạo RMT TX channel và gán copy encoder.
- `esp_err_t ir_engine_set_carrier(uint32_t freq_hz, float duty_cycle);`
  Cập nhật tần số sóng mang và duty cycle động.
- `esp_err_t ir_engine_send_raw(const void *symbols, size_t count);`
  Truyền chuỗi ký hiệu RMT ra LED phát (chờ truyền xong tối đa 3 giây).

### RX APIs
- `esp_err_t ir_engine_rx_init(const ir_rx_engine_config_t *config);`
  Khởi tạo RMT RX channel, thiết lập chân GPIO pullup và đăng ký ISR callback.
- `esp_err_t ir_engine_rx_start(void *buffer, size_t buffer_size_bytes);`
  Kích hoạt thu xung vào buffer được chỉ định (tự động bật enable channel nếu đang tắt).
- `esp_err_t ir_engine_rx_stop(void);`
  Tạm dừng / ngắt chế độ thu của kênh RX.
