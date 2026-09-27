<div align="center">

  <a href="https://github.com/dangminhtam/NexusIR">
    <img alt="Thiết bị NexusIR" src="hardware/3d_print/white.png" width="38%" />
  </a>

  <h1>NexusIR 🛸 Hub Điều Khiển IoT & Hồng Ngoại Đa Năng cho ESP32</h1>

  <h3>📡 Lõi Học Lệnh IR Core V3 · 🏠 Apple HomeKit & ESP RainMaker · ⚡ Event-Driven Engine · 🌐 Mạng Mesh ESP-NOW</h3>

  <p>
    <a href="https://www.espressif.com">
      <img src="https://img.shields.io/badge/chạy_trên-Dòng_ESP32-red?style=flat-square" alt="Chạy trên dòng ESP32" />
    </a>
    <a href="https://github.com/espressif/esp-idf">
      <img src="https://img.shields.io/badge/framework-ESP--IDF_v5.5-blue?style=flat-square" alt="ESP-IDF v5.5" />
    </a>
    <a href="./LICENSE">
      <img src="https://img.shields.io/github/license/espressif/esp-claw?style=flat-square" alt="Giấy phép" />
    </a>
    <img src="https://img.shields.io/badge/firmware-v1.5.8-green?style=flat-square" alt="Phiên bản Firmware" />
    <img src="https://img.shields.io/badge/build-passing-brightgreen?style=flat-square" alt="Trạng thái Build" />
  </p>

  <a href="#-tính-năng-nổi-bật">Tính Năng Chính</a>
  |
  <a href="#-nạp-firmware-trên-trình-duyệt-web-flasher">Nạp Trực Tiếp (Web Flasher)</a>
  |
  <a href="#-khởi-động-nhanh">Khởi Động Nhanh</a>
  |
  <a href="#-kiến-trúc--ngân-sách-bộ-nhớ">Kiến Trúc & Bộ Nhớ</a>
  |
  <a href="#-rest-api--web-dashboard">Web & API</a>
  |
  <a href="#-kiểm-thử-hồi-quy-host">Kiểm Thử</a>
  |
  <a href="./README.md">🇬🇧 English</a>

</div>

---

**NexusIR** là trung tâm điều khiển nhà thông minh (Smart IoT Hub) cấp độ doanh nghiệp, được xây dựng theo kiến trúc hướng sự kiện trên nền tảng **ESP-IDF v5.5** dành cho các dòng vi điều khiển ESP32 (ESP32 / ESP32-C3 / ESP32-S3). Hệ thống kết nối liền mạch các thiết bị hồng ngoại gia dụng (Điều hòa AC, Quạt, TV), hệ thống chiếu sáng RGB địa chỉ đa vùng, rơ-le công suất cao và cảm biến môi trường vào hệ sinh thái **Apple HomeKit** và **ESP RainMaker** (Amazon Alexa & Google Assistant) — phản hồi nội mạng siêu tốc với độ trễ tính bằng mili-giây, mạng mesh ESP-NOW phân tán và bảng điều khiển Web offline tích hợp.

<div align="center">
  <video src="picture/video.mp4" width="85%" controls></video>
  <p><i>Video thực tế: Quá trình học lệnh hồng ngoại, ghép nối Apple HomeKit và đồng bộ chiếu sáng đa vùng</i></p>
</div>

---

## 🌟 Tính Năng Nổi Bật

Các remote thông minh truyền thống thường gặp sự cố học lệnh thiếu chính xác, giải mã xung bị suy hao và phụ thuộc chặt chẽ vào đám mây. NexusIR giới thiệu **Kiến Trúc IR Core V3**: động cơ hướng sự kiện, phân tách hoàn toàn lớp trừu tượng phần cứng (HAL), định dạng nhị phân 20 bytes đóng gói kèm mã kiểm tra toàn vẹn phần cứng CRC32, di trú NVS an toàn chống mất nguồn và điều chế sóng mang động.

<table align="center">
  <tr>
    <th><div align="center"> 📡 Lõi Hồng Ngoại IR Core V3 </div></th>
    <th><div align="center"> 🏠 Hệ Sinh Thái Kép Tự Nhiên </div></th>
  </tr>
  <tr>
    <td>
      <div align="center">
        Ngoại vi RMT với độ chính xác thời gian 1 microsecond
        <br />
        Học chuỗi xung RAW không suy hao (Ma trận Điều hòa & Quạt)
        <br />
        Tự động điều chế sóng mang động (36 kHz tới 56 kHz)
      </div>
    </td>
    <td>
      <div align="center">
        Hỗ trợ trực tiếp giao thức Apple HomeKit (HAP)
        <br />
        ESP RainMaker cho Android, Alexa và Google Assistant
        <br />
        Hoạt động độc lập, không cần thiết bị Bridge ngoài hay phí cloud
      </div>
    </td>
  </tr>
  <tr>
    <td width="50%">
      <video src="picture/video.mp4" width="100%" controls></video>
    </td>
    <td width="50%">
      <img src="docs/images/result2.png" width="100%" alt="Giao diện Web NexusIR" />
    </td>
  </tr>

  <tr>
    <td colspan="2"><!-- spacer row --></td>
  </tr>

  <tr>
    <th><div align="center"> ⚙️ Kiến Trúc FreeRTOS Hướng Sự Kiện </div></th>
    <th><div align="center"> 🌐 Mạng Mesh & Đồng Bộ Phân Tán </div></th>
  </tr>
  <tr>
    <td>
      <div align="center">
        Ngắt RMT RX siêu tinh gọn (&lt; 1.5 µs) đẩy việc cho Worker Task
        <br />
        Session ID tăng đơn điệu triệt tiêu hoàn toàn sự kiện trễ
        <br />
        Hardware Watchdog 15 giây tự động hủy học tránh treo task
      </div>
    </td>
    <td>
      <div align="center">
        Cấu trúc mạng lưới Master / Slave độ trễ cực thấp qua ESP-NOW
        <br />
        Truyền lệnh ngang hàng giữa các phòng dưới 10ms
        <br />
        Tùy biến linh hoạt: Master, Slave, Standalone hoặc Vô hiệu hóa
      </div>
    </td>
  </tr>

  <tr>
    <td colspan="2"><!-- spacer row --></td>
  </tr>

  <tr>
    <th><div align="center"> 🧬 Định Dạng V1 & Di Trú An Toàn </div></th>
    <th><div align="center"> 💡 Chiếu Sáng Đa Vùng & Cảm Biến </div></th>
  </tr>
  <tr>
    <td>
      <div align="center">
        Header 20 bytes đóng gói chặt chẽ với kiểm tra lỗi IEEE 802.3 CRC32
        <br />
        Di trú tự động On-Access và Quét nền lúc khởi động cho NVS cũ
        <br />
        An toàn khi mất nguồn: ghi đè nguyên tử bảo vệ dữ liệu 100%
      </div>
    </td>
    <td>
      <div align="center">
        Điều khiển lên tới 5 dải LED địa chỉ WS2812B độc lập
        <br />
        Đo đạc và cập nhật nhiệt độ & độ ẩm liên tục qua cảm biến AHT20
        <br />
        2 kênh rơ-le công suất cao kèm ngắt nút bấm chạm cảm ứng
      </div>
    </td>
  </tr>

  <tr>
    <td colspan="2"><!-- spacer row --></td>
  </tr>

  <tr>
    <th><div align="center"> 🧰 Sẵn Sàng Vận Hành Ngay </div></th>
    <th><div align="center"> 🧩 Phân Tách HAL Tuyệt Đối </div></th>
  </tr>
  <tr>
    <td>
      <div align="center">
        Giao diện Web Dashboard nén gzip phục vụ trực tiếp từ chip
        <br />
        API Export / Import JSON giúp sao lưu và sao chép lệnh dễ dàng
        <br />
        Vỏ hộp vòm trong suốt hồng ngoại 360 độ (file .3mf sẵn sàng in)
      </div>
    </td>
    <td>
      <div align="center">
        Phân tách rõ ràng: Logic (Manager) vs Phần cứng (Driver HAL)
        <br />
        Xóa bỏ toàn bộ <code>ESP_ERROR_CHECK()</code> giúp hệ thống chạy 24/7
        <br />
        Timeout an toàn 3 giây khi phát RMT TX chống treo phần cứng
      </div>
    </td>
  </tr>
</table>

---

## ⚡ Nạp Firmware Trên Trình Duyệt (Web Flasher)

Bạn có thể cài đặt hoặc cập nhật NexusIR trực tiếp trên trình duyệt Web mà không cần cài driver hay công cụ dòng lệnh:

<div align="center">
  <a href="https://Hdchipeo.github.io/NexusIR/flash/">
    <img src="docs/images/flash-via-browser-button.svg" width="220" alt="Nạp Trực Tiếp Qua Trình Duyệt" />
  </a>
  <p>
    👉 <a href="https://Hdchipeo.github.io/NexusIR/flash/"><strong>Mở Trình Nạp NexusIR Web Flasher</strong></a>
  </p>
</div>

> [!IMPORTANT]
> **Trình duyệt hỗ trợ:** Google Chrome, Microsoft Edge, Opera trên Windows, macOS và Linux (hỗ trợ Web Serial API). Chỉ cần cắm cáp USB-C nối ESP32-C3 vào máy tính và bấm nút phía trên!

---

## 📦 Khởi Động Nhanh

<div align="center">
  <img src="hardware/3d_print/sanpham2.png" width="75%" alt="Bố trí phần cứng bên trong NexusIR" />
</div>

NexusIR tương thích với tất cả các dòng chip ESP32 có ngoại vi RMT (ESP32, ESP32-C3, ESP32-S3). Mặc định dưới đây được tối ưu cho **ESP32-C3**:

### Bảng Sơ Đồ Chân GPIO Mặc Định

| Ngoại vi | GPIO Mặc Định | Loại Driver | Mô tả Chức năng |
| :--- | :---: | :---: | :--- |
| **Phát Hồng Ngoại (TX)** | `GPIO 4` | RMT Output | Kích hoạt LED hồng ngoại qua mạch lái NPN/MOSFET |
| **Thu Hồng Ngoại (RX)** | `GPIO 7` | RMT Input | Mắt thu giải điều chế TSOP38238 (Kéo trở PULLUP nội) |
| **Đèn LED RGB 1–5** | `2, 8, 9, 10, 18` | RMT / SPI | Đường truyền dữ liệu điều khiển LED dải WS2812B |
| **Bus I2C (Cảm biến AHT20)** | `SDA=6, SCL=5` | I2C Master | Đọc dữ liệu nhiệt độ & độ ẩm môi trường (400 kHz) |
| **Rơ-le 1 / 2** | `GPIO 12, 13` | GPIO Output | Điều khiển cuộn hút rơ-le đóng cắt tải điện |
| **Nút Chạm Cảm Ứng** | `GPIO 14, 15` | GPIO Input | Ngắt kích hoạt nút bấm cảm ứng điện dung |
| **Nút Hệ Thống** | `GPIO 3` | GPIO Input | Khôi phục cài đặt gốc & chuyển chế độ cấu hình |

---

### Nạp Firmware Biên Dịch Sẵn (CLI)

Nếu bạn quen dùng dòng lệnh Terminal, các file nhị phân sẵn có trong thư mục [`firmware/`](firmware/):

```bash
# Ví dụ: Nạp NexusIR cho ESP32-C3 (Bản Apple HomeKit)
python -m esptool --chip esp32c3 -b 460800 write_flash \
  0x00000 firmware/esp32c3/ios/bootloader.bin \
  0x08000 firmware/esp32c3/ios/partition-table.bin \
  0x15000 firmware/esp32c3/ios/ota_data_initial.bin \
  0x20000 firmware/esp32c3/ios/nexus-ir.bin \
  0x3e0000 firmware/esp32c3/ios/storage.bin
```

> [!TIP]
> Thay đổi đường dẫn `ios` thành `android` để nạp bản dành cho **ESP RainMaker** (Android, Alexa, Google Home).

---

### Biên Dịch Từ Mã Nguồn (Build from Source)

Yêu cầu cài đặt **ESP-IDF v5.5.x**:

```bash
# 1. Kích hoạt môi trường ESP-IDF
. ~/esp/esp-idf/export.sh

# 2. Thiết lập chip mục tiêu
idf.py set-target esp32c3    # Hoặc esp32 / esp32s3

# 3. Tùy biến tính năng (Hệ sinh thái, GPIO, cấu hình ESP-NOW)
idf.py menuconfig

# 4. Biên dịch, nạp và mở log giám sát
idf.py build
idf.py -p /dev/tty.usbserial-XXXX flash monitor
```

---

### Cấu Hình Wi-Fi & Ghép Nối Nhà Thông Minh

<table align="center">
  <tr>
    <th width="50%"><div align="center"> 🍎 Apple HomeKit (iOS) </div></th>
    <th width="50%"><div align="center"> 🤖 ESP RainMaker (Android / Alexa / Google) </div></th>
  </tr>
  <tr>
    <td>
      <ol>
        <li>Kết nối vào Wi-Fi phát từ thiết bị: <code>NexusIR-Setup-XXXX</code></li>
        <li>Trang cấu hình Captive Portal tự mở &rarr; Chọn Wi-Fi gia đình & nhập mật khẩu.</li>
        <li>Mở ứng dụng Apple <b>Home</b> &rarr; Bấm <b>+</b> &rarr; <b>Thêm Phụ Kiện</b> &rarr; <b>Tùy Chọn Khác</b>.</li>
        <li>Chọn <b>NexusIR Bridge</b> &rarr; Nhập Mã Cài Đặt: <code>111-22-333</code>.</li>
      </ol>
    </td>
    <td>
      <ol>
        <li>Mở ứng dụng <b>ESP RainMaker</b> trên điện thoại.</li>
        <li>Bấm <b>Add Device</b> &rarr; Quét mã QR trên thiết bị hoặc dò qua BLE / SoftAP.</li>
        <li>Nhập mã xác nhận (PoP): <code>12345678</code>.</li>
        <li>Liên kết kỹ năng RainMaker trong ứng dụng Alexa / Google Home để ra lệnh giọng nói.</li>
      </ol>
    </td>
  </tr>
</table>

---

## 🏗️ Kiến Trúc & Ngân Sách Bộ Nhớ

### Cấu Trúc Nhị Phân Wire Format V1

Mọi lệnh học được lưu trữ dưới dạng cấu trúc nhị phân gói gọn `#pragma pack(push, 1)`:

```
+-----------------------------------------------------------------------+
| Magic (4B)  | Ver (1B) | Flags (1B) | Carrier (2B) | Duty (1B) | Rep (1B) |
| 0x5249584E  |   0x01   | Bitmask    |  38000 Hz    |    33%    |  0 or 1  |
+-----------------------------------------------------------------------+
| Gap (2B)    | DurCount (2B) |       CRC32 (4B, Little-Endian)         |
| 40 ms       | N halfwords   | esp_rom_crc32_le trên toàn bộ payload   |
+-----------------------------------------------------------------------+
| Payload: uint16_t durations[DurCount]                                |
| [Mark 0, Space 0, Mark 1, Space 1, ... Mark N-1]                      |
+-----------------------------------------------------------------------+
```

### Bảng Định Lượng Ngân Sách NVS (IR-12)

Phân vùng `nvs` dành riêng **32 KB** (8 Flash pages × 4096B), trong đó không gian khả dụng cho người dùng là ~27.5 KB:

| Thiết Bị | Số Xung Trung Bình | Độ Lớn Payload | Kích Thước Bản Ghi V1 | Ô Nhớ NVS | Ước Tính Sức Chứa NVS 32KB |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **TV / Quạt (NEC / RC5)** | 32 - 68 xung | 64 - 136 B | **84 - 156 bytes** | ~3 - 5 entries | **180 - 220 phím** |
| **Điều Hòa (AC Matrix)** | 100 - 240 xung | 200 - 480 B | **220 - 500 bytes** | ~7 - 16 entries | **35 - 45 phím AC** |

> [!TIP]
> Một bộ ma trận điều hòa đầy đủ (16 trạng thái nhiệt độ: Tắt, 16°C – 30°C) chiếm khoảng **~5.6 KB**. Cùng với 20 phím Quạt/TV (~2.5 KB), tổng dung lượng chiếm dụng chỉ là **~8.1 KB / 27.5 KB (~30%)**, để dành tới 70% không gian cho các chức năng khác của hệ thống.

---

## 🌐 REST API & Web Dashboard

NexusIR tích hợp sẵn bảng điều khiển Web nội bộ nén gzip truy cập tại `http://nexusir-xxxx.local`.

<div align="center">
  <img src="docs/images/result2.png" width="80%" alt="Giao diện Web NexusIR" />
</div>

### Bảng Tra Cứu HTTP API

| Đường dẫn (Endpoint) | Giao thức | Mã Phản Hồi | Mô tả Chức năng |
| :--- | :---: | :---: | :--- |
| `/api/learn/status` | `GET` | `200` | Lấy trạng thái phiên học (`ARMED`, `CAPTURED`, `TIMEOUT`), session ID và số xung |
| `/api/send?key={key}` | `POST` | `200, 404, 409, 500` | Phát lệnh IR. Trả về **409 Conflict** nếu thiết bị đang bận ở chế độ học lệnh |
| `/api/ir/export?key={key}` | `GET` | `200, 404` | Xuất thông số key (carrier, repeats, CRC32) và mảng xung micro giây dạng JSON |
| `/api/ir/import?key={key}` | `POST` | `200, 400` | Xác thực, chuẩn hóa, đóng gói V1 kèm CRC32 và ghi key nhập vào NVS |
| `/api/ir/storage` | `GET` | `200` | Trả về thống kê bộ nhớ NVS: số ô đã dùng, ô còn trống và ước lượng vị trí còn lại |
| `/api/ir/migrate` | `POST` | `200` | Kích hoạt quét toàn bộ phân vùng NVS và nâng cấp các phím legacy lên V1 |

---

## 🧪 Kiểm Thử Hồi Quy Host

Để rút ngắn thời gian phát triển và loại bỏ nguy cơ lỗi hồi quy, thuật toán xử lý chuỗi xung được phân tách độc lập khỏi kernel RTOS, cho phép biên dịch và chạy trực tiếp trên **macOS / Linux** trong **dưới 50 mili-giây**:

```bash
# Biên dịch và chạy bộ test hồi quy trên máy Host
clang -O2 -Wall -Wextra -Icomponents/mgr_ir_protocols/include \
  test/test_ir_waveform_host.c -o test/test_ir_waveform_host && ./test/test_ir_waveform_host
```

```text
=====================================================
  Running NexusIR Waveform Engine Host Test Suite    
=====================================================
  [PASS] test_waveform_normalization
  [PASS] test_odd_duration_overread_prevention
  [PASS] test_v1_serialization_and_deserialization
  [PASS] test_crc32_corruption_detection
  [PASS] test_legacy_raw_upgrade
  [PASS] test_legacy_matrix_upgrade
=====================================================
  ALL 6 HOST UNIT TESTS PASSED SUCCESSFULLY!         
=====================================================
```

---

## 🖨️ Thiết Kế Vỏ Hộp 3D

NexusIR sở hữu thiết kế dạng vòm bán cầu tối ưu hóa khả năng **phát xạ hồng ngoại đẳng hướng 360°**. Các tệp tin in 3D định dạng `.3mf` sẵn có tại [`hardware/3d_print/`](hardware/3d_print/):

<table align="center">
  <tr>
    <th><div align="center"> Đen Mờ </div></th>
    <th><div align="center"> Trắng Ngọc Trai </div></th>
    <th><div align="center"> Cam Cyber </div></th>
    <th><div align="center"> Hồng Pastel </div></th>
  </tr>
  <tr>
    <td><img src="hardware/3d_print/Black.png" width="180" alt="Đen" /></td>
    <td><img src="hardware/3d_print/white.png" width="180" alt="Trắng" /></td>
    <td><img src="hardware/3d_print/Orange.png" width="180" alt="Cam" /></td>
    <td><img src="hardware/3d_print/Pink.png" width="180" alt="Hồng" /></td>
  </tr>
</table>

---

## 📷 Theo Dõi Dự Án

Nếu NexusIR hữu ích với ngôi nhà của bạn, hãy dành tặng dự án một ngôi sao nhé! ⭐

<div align="center">
  <a href="https://www.star-history.com/?repos=dangminhtam%2FNexusIR&type=date&legend=top-left">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=dangminhtam%2FNexusIR&type=date&theme=dark&legend=top-left" />
      <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=dangminhtam%2FNexusIR&type=date&legend=top-left" />
      <img alt="Lịch sử Star NexusIR" src="https://api.star-history.com/chart?repos=dangminhtam%2FNexusIR&type=date&legend=top-left" width="80%" />
    </picture>
  </a>
</div>

---

## Lời Cảm Ơn

- [Espressif Systems](https://github.com/espressif) vì nền tảng ESP-IDF tuyệt vời và driver RMT mạnh mẽ.
- [HomeKit ADK & ESP-HomeKit-SDK](https://github.com/espressif/esp-homekit-sdk) hỗ trợ giao thức Apple HomeKit.
- [ESP RainMaker](https://rainmaker.espressif.com/) tích hợp Android và các trợ lý ảo giọng nói.
- [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) làm tham chiếu định dạng xung hồng ngoại.
- Cảm hứng thiết kế và tiêu chuẩn tài liệu mở từ Espressif ([esp-claw](https://github.com/espressif/esp-claw)).

---

## Giấy Phép

Dự án được phân phối mã nguồn mở theo giấy phép **MIT License**. Xem chi tiết tại [LICENSE](LICENSE).
