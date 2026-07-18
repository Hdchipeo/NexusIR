# NexusIR — Bộ Điều Khiển IoT Thông Minh cho ESP32

[🇬🇧 English](README.md)

**NexusIR** là hub IoT modular xây dựng trên **ESP-IDF** cho ESP32 / ESP32-C3 / ESP32-S3. Tích hợp điều khiển IR (điều hòa, quạt), LED RGB WS2812B, relay và cảm biến nhiệt độ vào **Apple HomeKit** hoặc **ESP RainMaker** — điều khiển qua Siri, Alexa, Google Assistant, hoặc Web UI cục bộ.

<p align="center">
  <img src="hardware/3d_print/white.png" width="280" alt="Thiết bị NexusIR">
  <img src="hardware/3d_print/Case.png" width="280" alt="Bản vẽ phân tách">
  <img src="hardware/3d_print/sanpham2.png" width="280" alt="PCB bên trong">
</p>

---

## Tính Năng Chính

| Danh mục | Chi tiết |
|----------|----------|
| **Nền tảng** | Apple HomeKit (iOS) · ESP RainMaker (Android/Alexa/Google) |
| **Điều khiển IR** | Điều hòa + Quạt — đa thương hiệu, học lệnh IR qua Web UI |
| **Đèn LED** | Tối đa 5× dải WS2812B RGB — màu sắc, độ sáng, hiệu ứng |
| **Relay** | 2× relay đầu ra với nút cảm ứng |
| **Cảm biến** | AHT20 nhiệt độ & độ ẩm |
| **Kết nối** | ESP-NOW Master/Slave mesh · Wi-Fi · mDNS · OTA cập nhật |
| **Web UI** | Dashboard tích hợp — nén gzip, phục vụ từ SPIFFS |

---

## Kiến Trúc Hệ Thống

Mỗi thiết bị ngoại vi có thể cấu hình là **Master**, **Slave**, **Standalone**, hoặc **Disabled** qua `idf.py menuconfig`:

```
┌─────────────────────────────────────────────────────────┐
│  iOS Home App / RainMaker App / Web UI                  │
└────────────────────────┬────────────────────────────────┘
                         │ Wi-Fi / HAP
               ┌─────────▼──────────┐
               │   Master Node      │
               │  HomeKit + Sync    │
               │  Engine + Web UI   │
               └──┬──────────────┬──┘
        ESP-NOW   │              │   ESP-NOW
           ┌──────▼────┐   ┌────▼───────┐
           │ Slave: AC  │   │ Slave: LED │
           │ IR TX/RX   │   │ WS2812B ×5 │
           └────────────┘   └────────────┘
```

- **Master** — Hiển thị accessory lên HomeKit/RainMaker, chuyển lệnh qua ESP-NOW đến Slave.
- **Slave** — Nhận lệnh ESP-NOW, điều khiển phần cứng cục bộ (IR, LED, Relay).
- **Standalone** — Tất cả trên một chip, không cần ESP-NOW.
- **Disabled** — Loại bỏ hoàn toàn khỏi firmware, tiết kiệm RAM/Flash.

---

## Phần Cứng — Sơ Đồ GPIO Mặc Định

> Tất cả chân cấu hình được trong `menuconfig`. Giá trị mặc định cho ESP32-C3.

| Ngoại vi | GPIO | Kiểu |
|----------|------|------|
| IR Phát | 4 | RMT Output |
| IR Thu | 7 | RMT Input |
| LED Lamp 1–5 | 2, 8, 9, 10, 18 | RMT / PWM |
| I2C (AHT20) | SDA=6, SCL=5 | Open Drain |
| Relay 1 / 2 | 12, 13 | Output |
| Nút cảm ứng 1 / 2 | 14, 15 | Input |
| Nút hệ thống | 3 | Input |

---

## Bắt Đầu Nhanh

### 1. Build & Flash

```bash
# Khởi tạo môi trường ESP-IDF
. ~/esp/esp-idf/export.sh

# Chọn chip mục tiêu
idf.py set-target esp32c3    # hoặc esp32, esp32s3

# Cấu hình tính năng
idf.py menuconfig

# Build và flash
idf.py build
idf.py -p /dev/tty.usbserial-XXXX flash monitor
```

### 2. Cấu Hình Wi-Fi

| Nền tảng | Cách thực hiện |
|----------|----------------|
| **iOS** | Kết nối Wi-Fi `NexusIR-Setup-XXXX` → trang cấu hình tự mở → chọn mạng Wi-Fi → nhập mật khẩu |
| **Android** | Mở ESP RainMaker → Thêm thiết bị → quét QR hoặc chọn BLE/SoftAP → PoP: `12345678` |

### 3. Ghép Nối Smart Home

| Nền tảng | Các bước |
|----------|----------|
| **iOS HomeKit** | Home App → "+" → Thêm phụ kiện → Tùy chọn khác → NexusIR Bridge → Mã cài đặt: `111-22-333` |
| **RainMaker** | Thiết bị tự xuất hiện sau provisioning. Liên kết Alexa/Google qua RainMaker skill. |

**Điều khiển giọng nói:**
- *"Hey Siri, tắt điều hòa phòng khách."*
- *"Alexa, đặt đèn 1 màu xanh."*
- *"OK Google, nhiệt độ phòng ngủ bao nhiêu?"*

---

## Tổng Quan Menuconfig

Chạy `idf.py menuconfig` để truy cập:

| Menu | Tùy chọn chính |
|------|----------------|
| **Device Configuration** | AC, Quạt, Cảm biến, LED 1–5, Relay — chế độ (Master/Slave/Standalone/Disabled), GPIO, tên |
| **ESP-NOW Configuration** | Tên thiết bị, MAC đồng bộ, thứ tự màu LED & độ sáng tối đa |
| **Mobile Platform** | iOS (HomeKit) hoặc Android (RainMaker) |
| **WiFi Configuration** | Phương thức provisioning (SoftAP/BLE), AP dự phòng, mDNS hostname |
| **OTA Configuration** | Bật/tắt tự cập nhật, URL server, chu kỳ kiểm tra |
| **Weather Configuration** | Bật/tắt dịch vụ thời tiết Open-Meteo |

---

## Firmware Biên Dịch Sẵn

Flash trực tiếp không cần build — binary trong thư mục [`firmware/`](firmware/):

```
firmware/
├── esp32/
│   ├── android/    # Bản RainMaker
│   └── ios/        # Bản HomeKit
└── esp32c3/
    ├── android/
    └── ios/
```

**Lệnh flash (ESP32-C3, bản iOS):**

```bash
python -m esptool --chip esp32c3 -b 460800 write_flash \
  0x0     firmware/esp32c3/ios/bootloader.bin \
  0x8000  firmware/esp32c3/ios/partition-table.bin \
  0x15000 firmware/esp32c3/ios/ota_data_initial.bin \
  0x20000 firmware/esp32c3/ios/nexus-ir.bin \
  0x3e0000 firmware/esp32c3/ios/storage.bin
```

> Thay `ios` bằng `android` cho bản RainMaker.

---

## Web UI

Dashboard cục bộ tích hợp cho học lệnh IR, hiệu ứng LED, và cấu hình thiết bị.

- **iOS**: Tự phát hiện qua mDNS → `http://nexusir-xxxx.local`
- **Android**: Bật "WebUI Config Mode" trong RainMaker app trước.

**Biên dịch lại web assets:**

```bash
./components/svc_web_server/compress_web.sh
idf.py build
```

---

## Vỏ In 3D

Thiết kế dạng vòm với khả năng truyền IR 360°. File in 3D (`.3mf`) trong [`hardware/3d_print/`](hardware/3d_print/).

| Đen | Trắng | Cam | Hồng |
|:---:|:-----:|:---:|:----:|
| ![Đen](hardware/3d_print/Black.png) | ![Trắng](hardware/3d_print/white.png) | ![Cam](hardware/3d_print/Orange.png) | ![Hồng](hardware/3d_print/Pink.png) |

---

## Giấy Phép

MIT License — xem [LICENSE](LICENSE) để biết chi tiết.
