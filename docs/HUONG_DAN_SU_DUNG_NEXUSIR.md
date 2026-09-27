# NexusIR Smart Hub

### Hướng dẫn sử dụng

**Trung tâm điều khiển hồng ngoại và ánh sáng thông minh cho hệ sinh thái Apple HomeKit & ESP RainMaker.**

![NexusIR Smart Hub](images/hero-hub.png)

Phiên bản tài liệu: **1.1**  
Ngày phát hành: **23/09/2026**  
Áp dụng cho: **NexusIR Firmware v1.4.0 trở lên (ESP32 / ESP32-C3 / ESP32-S3)**

---

## Nội dung

1. [Bắt đầu trong 5 phút](#1-bắt-đầu-trong-5-phút)
2. [Trước khi sử dụng](#2-trước-khi-sử-dụng)
3. [Làm quen với sản phẩm](#3-làm-quen-với-sản-phẩm)
4. [Thiết lập lần đầu](#4-thiết-lập-lần-đầu)
5. [Sử dụng hằng ngày](#5-sử-dụng-hằng-ngày)
6. [Học lệnh hồng ngoại (IR Learning) cho Điều hòa & Quạt trên Web Config](#6-học-lệnh-hồng-ngoại-ir-learning-cho-điều-hòa--quạt-trên-web-config)
7. [Tùy chỉnh và bảo quản](#7-tùy-chỉnh-và-bảo-quản)
8. [Xử lý sự cố](#8-xử-lý-sự-cố)
9. [Câu hỏi thường gặp](#9-câu-hỏi-thường-gặp)
10. [Thông số và hỗ trợ](#10-thông-số-và-hỗ-trợ)

---

## 1. Bắt đầu trong 5 phút

Chỉ với 4 bước đơn giản, bạn sẽ kích hoạt NexusIR và sẵn sàng điều khiển điều hòa, quạt và hệ thống đèn LED thông qua ứng dụng Apple Home hoặc giọng nói với Siri.

1. **Cấp nguồn:** Cắm cáp USB Type-C (5V/2A) vào NexusIR. Đèn LED báo hiệu sẽ sáng lên.
2. **Kết nối mạng cài đặt:** Mở Wi-Fi trên iPhone, chọn mạng **NexusIR-Setup-XXXX** (hoặc *GhostMagic-Setup-XXXX*).
3. **Cấu hình Wi-Fi:** Trang đăng nhập sẽ tự mở. Chọn Wi-Fi 2.4GHz nhà bạn, nhập mật khẩu và bấm **Join**. Thiết bị sẽ tự khởi động lại.
4. **Ghép nối Apple Home:** Mở ứng dụng **Nhà (Apple Home)**, chọn **(+) -> Thêm phụ kiện**, chọn *NexusIR Bridge* và nhập mã thiết lập: **`111-22-333`**.

**Hoàn tất khi:** Biểu tượng cầu nối NexusIR xuất hiện trên màn hình chính ứng dụng Apple Home, đèn LED trạng thái chuyển sang màu xanh dương ổn định.

---

## 2. Trước khi sử dụng

### Trong hộp có gì

- Thiết bị NexusIR Smart Hub × 1
- Cáp nguồn USB Type-C chống nhiễu (1.0m) × 1
- Thẻ hướng dẫn nhanh kèm mã QR Apple HomeKit × 1
- Băng keo định vị 3M chuyên dụng × 1

### Bạn cần chuẩn bị

- **Nguồn điện:** Củ sạc chuẩn USB 5V, tối thiểu 1.5A – 2A (đặc biệt khi kết nối với các dải đèn LED WS2812B).
- **Mạng Wi-Fi:** Băng tần **2.4 GHz** (802.11 b/g/n) đang hoạt động ổn định.
- **Thiết bị điều khiển:** iPhone hoặc iPad chạy iOS/iPadOS 15.0 trở lên, đã đăng nhập tài khoản iCloud và bật sẵn Bluetooth, Dịch vụ định vị.

> **Lưu ý quan trọng:** NexusIR không hỗ trợ băng tần Wi-Fi 5 GHz đơn thuần. Nếu bộ phát Wi-Fi nhà bạn dùng chung một tên (SSID) cho cả 2.4 GHz và 5 GHz, hãy tạm thời tắt gộp băng tần (Band Steering) hoặc tách riêng sóng 2.4 GHz trong lúc cài đặt.

---

## 3. Làm quen với sản phẩm

NexusIR sở hữu thiết kế hình khối vòm khí động học, bố trí mảng thấu kính quang học giúp phát tín hiệu hồng ngoại 360 độ bao phủ toàn bộ không gian phòng.

| Vị trí | Thành phần phần cứng | Chức năng hoạt động |
|:---:|---|---|
| **1** | **Vòm phát hồng ngoại 360° (IR Blaster)** | Mảng LED phát hồng ngoại 940nm công suất cao, điều khiển điều hòa, quạt, TV từ mọi góc độ. |
| **2** | **Mắt thu hồng ngoại (IR Receiver)** | Cảm biến thu tín hiệu 38 kHz, dùng để học lệnh trực tiếp từ các remote điều khiển vật lý. |
| **3** | **Cảm biến môi trường AHT20** | Cảm biến tích hợp đo nhiệt độ và độ ẩm phòng theo thời gian thực (chuẩn I2C). |
| **4** | **Cổng mở rộng LED WS2812B & Relay** | Kết nối tối đa 5 kênh dải đèn LED RGB lập trình và 2 kênh tiếp điểm relay điều khiển thiết bị điện. |
| **5** | **Nút bấm đa năng (System Button)** | Nhấn 1 lần: Đổi chế độ đèn; Nhấn giữ 5 giây: Xóa cài đặt Wi-Fi và đưa máy về chế độ cấu hình. |
| **6** | **Cổng nguồn Type-C** | Tiếp nhận nguồn điện DC 5V tiêu chuẩn. |

### Trạng thái đèn báo trên thiết bị

| Màu sắc & Kiểu nháy | Ý nghĩa trạng thái | Bạn cần làm gì |
|---|---|---|
| **Nhấp nháy vàng chậm** | Thiết bị đang ở chế độ chờ cài đặt (SoftAP). | Mở Wi-Fi điện thoại kết nối vào `NexusIR-Setup-XXXX`. |
| **Nhấp nháy xanh lá nhanh** | Đang kết nối vào mạng Wi-Fi gia đình. | Đợi 5–10 giây để thiết bị hoàn tất xác thực. |
| **Xanh dương sáng tĩnh** | Hoạt động bình thường, đã đồng bộ Apple HomeKit. | Sẵn sàng điều khiển qua Home app hoặc Siri. |
| **Nháy đỏ 2 lần** | Sai mật khẩu Wi-Fi hoặc mất kết nối router. | Kiểm tra lại mật khẩu Wi-Fi hoặc khởi động lại router. |
| **Nháy tím nhịp thở (Breathing)** | Đang nhận/học mã lệnh hồng ngoại qua Web UI. | Hướng remote vào thiết bị và bấm phím cần học. |

---

## 4. Thiết lập lần đầu

### Phần I: Cấu hình mạng Wi-Fi (Wi-Fi Provisioning)

Kết nối NexusIR vào mạng Wi-Fi gia đình để thiết bị hòa mạng cục bộ.

**Trước khi bắt đầu:** Cắm nguồn cho NexusIR, đảm bảo đèn báo nhấp nháy vàng chậm.

1. **Kết nối Wi-Fi thiết bị:** Trên iPhone, vào **Cài đặt (Settings) -> Wi-Fi**. Tìm và chọn mạng có tên **NexusIR-Setup-XXXX** (hoặc *GhostMagic-Setup-XXXX*).
2. **Xác nhận kết nối:** Chờ xuất hiện dấu tích xanh bên cạnh tên mạng. Thông báo *"Mạng không bảo mật"* hoặc *"Không có kết nối Internet"* là bình thường.
3. **Mở trang cấu hình:** Màn hình **Captive Portal** sẽ tự động hiển thị. Nếu không thấy, mở Safari và truy cập địa chỉ IP: **`192.168.4.1`**.
4. **Chọn Wi-Fi nhà bạn:** Trong danh sách mạng quét được, chạm vào tên Wi-Fi nhà bạn (băng tần 2.4GHz).
5. **Nhập mật khẩu & Kết nối:** Nhập chính xác mật khẩu vào ô, sau đó nhấn nút **Join**.
6. **Lưu cấu hình:** Màn hình sẽ hiện thông báo *"Success! Rebooting..."*. NexusIR lưu thông tin vào bộ nhớ Flash NVS và tự khởi động lại.

**Hoàn tất khi:** NexusIR phát tín hiệu đèn xanh lá rồi chuyển sang trạng thái sẵn sàng. Điện thoại của bạn tự động kết nối lại mạng Wi-Fi gia đình.

---

### Phần II: Đồng bộ với Apple HomeKit

Đưa toàn bộ thiết bị ngoại vi của NexusIR vào hệ sinh thái Apple Home.

**Trước khi bắt đầu:** Đảm bảo iPhone đang kết nối cùng mạng Wi-Fi gia đình với NexusIR.

1. **Thêm phụ kiện:** Mở ứng dụng **Nhà (Apple Home)** trên iPhone. Nhấn vào biểu tượng dấu **(+)** ở góc trên bên phải, chọn **Thêm phụ kiện (Add Accessory)**.
2. **Nhập mã thủ công:** Chọn dòng chữ màu xanh *"Tôi không có mã hoặc không thể quét"*, sau đó chọn **NexusIR Bridge** (hoặc *GhostLamp Bridge*).
3. **Điền mã ghép nối:** Nhập mã thiết lập gồm 8 chữ số: **`111-22-333`** (nhập liền `11122333`).
4. **Xác nhận kết nối:** Khi xuất hiện hộp thoại *"Phụ kiện chưa được chứng nhận"*, hãy nhấn chọn **Vẫn thêm (Add Anyway)**.
5. **Phân bổ phòng:** Chọn vị trí phòng cho Cầu nối (ví dụ: *Phòng khách*), nhấn **Tiếp tục**.
6. **Đặt tên phụ kiện:** Lần lượt đặt tên cho Điều hòa, Quạt, Cảm biến nhiệt độ và các dải đèn LED kết nối theo ý muốn.
7. **Hoàn tất:** Nhấn **Xong (Done)**.

**Hoàn tất khi:** Tất cả các biểu tượng (Điều hòa, Đèn, Cảm biến, Quạt) xuất hiện đầy đủ trong phòng tương ứng trên ứng dụng Apple Home.

---

## 5. Sử dụng hằng ngày

### Điều khiển qua ứng dụng Apple Home

- **Bật/Tắt tức thời:** Chạm nhẹ vào biểu tượng Đèn, Quạt hoặc Điều hòa để bật/tắt nhanh.
- **Điều chỉnh độ sáng & màu sắc:** Nhấn giữ vào biểu tượng Đèn để kéo thanh trượt độ sáng (0–100%) hoặc chọn màu sắc từ vòng xoay 16 triệu màu.
- **Điều chỉnh nhiệt độ điều hòa:** Nhấn giữ biểu tượng Điều hòa để xoay chỉnh nhiệt độ mong muốn (16°C – 30°C) hoặc đổi chế độ Làm mát (Cool), Sưởi (Heat), Tự động (Auto).
- **Theo dõi môi trường:** Nhiệt độ và độ ẩm phòng từ cảm biến AHT20 luôn được cập nhật liên tục trên thanh trạng thái đầu trang của ứng dụng Nhà.

### Ra lệnh bằng giọng nói qua Siri

Bạn có thể ra lệnh rảnh tay bằng tiếng Việt hoặc tiếng Anh trên iPhone, iPad, Apple Watch hoặc HomePod:

- *"Hey Siri, bật đèn phòng khách."*
- *"Hey Siri, chỉnh đèn phòng khách sang màu vàng ấm 50%."*
- *"Hey Siri, đặt điều hòa 24 độ."*
- *"Hey Siri, nhiệt độ phòng ngủ hiện tại là bao nhiêu?"*
- *"Hey Siri, tắt tất cả thiết bị khi tôi ra ngoài."*

---

## 6. Học lệnh hồng ngoại (IR Learning) cho Điều hòa & Quạt trên Web Config

NexusIR sở hữu công cụ Web Config trực quan, cho phép học lệnh chính xác từ bất kỳ remote vật lý nào (kể cả các thương hiệu quạt nội địa hoặc điều hòa hiếm gặp).

### 6.1. Cách truy cập Web Config

1. Mở trình duyệt (Safari hoặc Chrome) trên thiết bị cùng mạng Wi-Fi với hub.
2. Truy cập tên miền mDNS: **`http://nexusir-xxxx.local`** (với `xxxx` là 4 ký tự cuối MAC in trên tem) hoặc gõ địa chỉ IP nội bộ của hub (ví dụ: `http://192.168.1.50`).
3. *(Tùy chọn tiện lợi)*: Trong ứng dụng Apple Home, bạn có thể gạt bật công tắc ảo **Web Config** để hub phát tín hiệu nhận diện.

### 6.2. Chuẩn bị trước khi học lệnh

- Đặt remote gốc hướng thẳng vào mắt thu IR trên hub ở khoảng cách **5 cm – 10 cm**.
- Tránh ánh sáng mặt trời chiếu trực tiếp hoặc đèn huỳnh quang công suất cao rọi vào mắt thu để xung không bị nhiễu.
- Khi hub vào chế độ học lệnh, đèn LED hệ thống sẽ chuyển sang màu **Tím nhịp thở (Breathing Purple)**.

### 6.3. Quy trình học lệnh Quạt (FAN Wizard)

Quạt sử dụng cơ chế lưu lệnh đơn lẻ vào bộ nhớ NVS:

1. Trên Web Config, bấm vào nút **(+) Thêm thiết bị** -> Chọn **Quạt (FAN)**.
2. Nhập tên thiết bị (ví dụ: `Fan_LivingRoom`) và bấm **Tiếp theo**.
3. Hệ thống sẽ lần lượt yêu cầu bạn bấm các nút trên remote gốc:
   * **Nguồn:** Hướng remote vào hub và bấm nút Bật/Tắt quạt.
   * **Tốc độ 1, 2, 3:** Lần lượt bấm các nút số 1 (Chậm), 2 (Vừa), 3 (Mạnh).
   * **Đảo gió (Swing):** Bấm nút đảo hướng/chuyển hướng gió.
   *(Nếu remote không có phím nào, bấm nút **Bỏ qua (Skip)**).*
4. Sau khi học xong phím cuối cùng, hệ thống tự động ghi cấu hình vào NVS (`F_Fan_LivingRoom_...`) và kích hoạt thiết bị ngay trên Apple Home.

### 6.4. Quy trình học lệnh Điều hòa ma trận (AC Matrix Learning Wizard)

> **Hiểu đúng nguyên lý:** Remote điều hòa không gửi lệnh tăng/giảm đơn lẻ như TV hay quạt. Mỗi lần bấm phím, remote điều hòa phát đi **toàn bộ gói trạng thái** gồm: Nguồn, Nhiệt độ đặt, Chế độ (Cool/Heat), Tốc độ quạt và Hướng gió. Do đó, NexusIR sử dụng thuật toán **Ma trận xung nhị phân (Binary Pulse Matrix)** lưu trên phân vùng SPIFFS để tự động lập bản đồ từ 16°C đến 30°C.

1. Bấm **(+) Thêm thiết bị** -> Chọn **Điều hòa (AC)**.
2. Nhập tên điều hòa (ví dụ: `AC_MasterRoom`) -> Bấm **Tiếp theo**.
3. **Bước 0 — Tắt nguồn:** Hướng remote vào hub, bấm nút **TẮT** (Power Off). Hub xác nhận bắt được xung tắt máy.
4. **Bước 1 — Mốc chuẩn 16°C Làm mát (Rất quan trọng):**
   * **Mẹo kỹ sư:** Dùng tay che kín đầu phát hồng ngoại của remote, bật remote lên và chỉnh màn hình remote về đúng chế độ **COOL (Làm mát)** ở **16°C**.
   * Sau đó bỏ tay che, hướng remote vào hub và bấm một phím bất kỳ (hoặc phím Gửi/Nguồn) để hub ghi nhận mã chuẩn 16°C Cool.
5. **Bước 2 đến 15 — Quét ma trận dải nhiệt độ (17°C đến 30°C):**
   * Giữ nguyên remote hướng về phía NexusIR.
   * Khi màn hình Web Config hiển thị nhắc nhở nhiệt độ kế tiếp, bạn chỉ cần bấm **nút Tăng nhiệt độ (+)** trên remote một nấc.
   * Thanh tiến trình (Progress Bar) sẽ tự động nhảy lần lượt: `1/15`, `2/15`... cho đến `15/15` (hoàn tất mức 30°C).
6. **Lưu trữ & Tự động đồng bộ:**
   * Sau khi đạt 15/15, hub tự động biên dịch tệp nhị phân lưu vào `/spiffs/ir_matrix/AC_MasterRoom.bin`.
   * Phụ kiện Thermostat trên Apple HomeKit sẽ tự động nhận diện thương hiệu tùy chỉnh này. Giờ đây bạn có thể kéo thanh trượt trên iPhone hoặc ra lệnh *"Siri, đặt điều hòa 25 độ"*, hub sẽ trích xuất xung chuẩn từ ma trận và phát ra điều khiển tức thì.

---

## 7. Tùy chỉnh và bảo quản

### Bảng tùy chọn hệ thống

| Tùy chọn | Công dụng | Mặc định | Khi nào nên đổi |
|---|---|---|---|
| **Chế độ nút bấm** | Chọn hành vi khi ấn nút vật lý (Đổi hiệu ứng LED / Tắt mở Relay) | Đổi hiệu ứng LED | Khi muốn dùng nút trên hub như công tắc đèn bàn. |
| **Độ sáng tối đa LED** | Giới hạn dòng tiêu thụ của dải LED WS2812B | 80% (An toàn) | Đặt 100% nếu dùng củ nguồn chuẩn trên 3A. |
| **Chu kỳ đồng bộ cảm biến** | Thời gian gửi dữ liệu nhiệt ẩm AHT20 lên HomeKit | 30 giây | Rút ngắn xuống 10 giây nếu cần tự động hóa nhạy bén. |
| **Tên miền mDNS** | Tên miền truy cập Web UI cục bộ (`.local`) | `nexusir-xxxx` | Đổi thành tên dễ nhớ như `hub-phongkhach.local`. |

### Cập nhật phần mềm (OTA Update)

NexusIR trang bị hệ thống phân vùng kép an toàn (`ota_0` và `ota_1`), đảm bảo không bao giờ bị lỗi phần mềm (brick) khi cập nhật gián đoạn:

1. Truy cập Web UI tại địa chỉ `http://nexusir-xxxx.local`, chuyển sang tab **Cập nhật (Firmware Update)**.
2. Chọn tệp tin firmware mới nhất có định dạng `.bin` (hoặc nhấn nút **Kiểm tra bản cập nhật mới** qua Internet).
3. Nhấn **Bắt đầu cập nhật**. Thanh tiến trình sẽ chạy từ 0% đến 100%.
4. Sau khi ghi thành công, thiết bị sẽ tự khởi động lại vào phiên bản mới trong vòng 15 giây.

> **Cảnh báo:** Tuyệt đối không rút nguồn điện hoặc tắt router Wi-Fi trong quá trình nạp firmware OTA.

### Vệ sinh và bảo quản

- Đặt thiết bị ở nơi thoáng khí, cách các vật cản lớn ít nhất 30 cm để góc phát hồng ngoại 360 độ đạt hiệu quả tối ưu.
- Dùng khăn sợi nhỏ (microfiber) khô lau nhẹ bề mặt vòm phát hồng ngoại. Không dùng cồn 90 độ hoặc dung dịch tẩy rửa mạnh làm mờ bề mặt thấu kính quang học.
- Tránh đặt thiết bị dưới ánh nắng trực tiếp hoặc nơi có độ ẩm cao trên 85% RH kéo dài.

---

## 8. Xử lý sự cố

| Hiện tượng | Nguyên nhân có thể | Cách xử lý từng bước |
|---|---|---|
| **Không lên nguồn, đèn tắt hoàn toàn** | Cáp nguồn lỏng hoặc củ sạc không đủ điện áp | 1. Cắm lại chắc chắn đầu cáp Type-C vào thiết bị.<br/>2. Đổi sang củ sạc 5V/2A khác và thử lại. |
| **Không thấy Wi-Fi `NexusIR-Setup-XXXX`** | Thiết bị đã kết nối mạng cũ hoặc chưa vào chế độ cài đặt | 1. Cắm nguồn cho thiết bị.<br/>2. **Nhấn và giữ nút vật lý trên hub trong 5 giây** cho đến khi đèn nháy vàng nhanh rồi thả tay. Hub sẽ phát lại Wi-Fi Setup. |
| **Không hiện trang Captive Portal cấu hình** | Trình duyệt điện thoại chặn cửa sổ tự bật | 1. Đảm bảo điện thoại đã kết nối vào mạng `NexusIR-Setup-XXXX`.<br/>2. Mở trình duyệt Safari/Chrome, nhập thủ công địa chỉ **`192.168.4.1`** và nhấn Go. |
| **Báo lỗi khi kết nối Wi-Fi nhà** | Nhập sai mật khẩu hoặc router chỉ phát băng tần 5GHz | 1. Kiểm tra kỹ chữ hoa, chữ thường của mật khẩu.<br/>2. Kiểm tra bộ phát Wi-Fi: Bắt buộc chọn SSID băng tần 2.4GHz. |
| **Apple Home báo "Không thể thêm phụ kiện"** | Điện thoại khác mạng Wi-Fi hoặc phụ kiện bị kẹt cache HAP | 1. Đảm bảo iPhone đang kết nối cùng mạng Wi-Fi 2.4GHz với hub.<br/>2. Khởi động lại ứng dụng Home hoặc tắt/bật lại Wi-Fi trên iPhone.<br/>3. Nhập chính xác mã: **`111-22-333`**. |
| **Điều hòa/Quạt không nhận tín hiệu IR** | Hướng phát bị che khuất hoặc chưa đúng mã remote | 1. Chỉnh lại vị trí NexusIR hướng về phía mắt nhận của điều hòa.<br/>2. Vào Web UI kích hoạt tính năng học lại remote gốc để nạp chuẩn mã xung IR. |

**Vẫn chưa giải quyết được?** Ghi lại trạng thái đèn LED báo hiệu, mở Web UI chụp màn hình tab *Hệ thống (System Info)* và liên hệ với đội ngũ hỗ trợ kỹ thuật bên dưới.

---

## 9. Câu hỏi thường gặp

### 1. Nếu nhà bị mất mạng Internet, NexusIR có hoạt động không?
**Có.** Toàn bộ việc điều khiển thông qua Apple HomeKit và Web UI đều xử lý 100% trong mạng nội bộ gia đình (Local Area Network). Bạn vẫn có thể bật tắt đèn, điều hòa bình thường khi mất kết nối Internet quốc tế.

### 2. Một thiết bị NexusIR có thể điều khiển được bao nhiêu điều hòa và quạt?
Một hub NexusIR có thể điều khiển tất cả các thiết bị hồng ngoại nằm trong cùng một phòng (không có tường chắn che khuất). Bạn có thể cấu hình điều khiển đồng thời 1 Điều hòa, 1 Quạt, 1 TV và 5 dải đèn LED độc lập.

### 3. Làm thế nào để điều khiển NexusIR khi ở ngoài nhà?
Bạn chỉ cần sở hữu một thiết bị làm **Trung tâm nhà (Home Hub)** của Apple (như Apple TV 4K, HomePod hoặc HomePod mini) đặt trong nhà. Ứng dụng Apple Home sẽ tự động kết nối từ xa qua iCloud mã hóa đầu cuối an toàn.

### 4. Tôi dùng điện thoại Android thì có điều khiển được không?
**Có.** NexusIR hỗ trợ nền tảng **ESP RainMaker** dành cho Android. Bạn có thể tải ứng dụng ESP RainMaker trên Google Play, quét mã QR để thêm thiết bị và đồng bộ trực tiếp với Google Assistant hoặc Amazon Alexa.

---

## 10. Thông số và hỗ trợ

### Bảng thông số kỹ thuật

| Thông số | Chi tiết kỹ thuật |
|---|---|
| **Tên sản phẩm** | NexusIR Smart Hub (Bộ Điều Khiển IoT Đa Năng) |
| **Mã sản phẩm** | NX-IR-01 (Phiên bản ESP32-C3 / ESP32-S3) |
| **Vi điều khiển trung tâm** | 32-bit RISC-V / Dual-core Xtensa, xung nhịp tối đa 240 MHz |
| **Nguồn điện đầu vào** | 5V DC ± 5%, cổng cắm USB Type-C (Tối thiểu 1.5A) |
| **Chuẩn kết nối không dây** | Wi-Fi 802.11 b/g/n (2.4 GHz) · Bluetooth LE 5.0 · ESP-NOW Mesh |
| **Giao thức Smart Home** | Apple HomeKit (HAP) · ESP RainMaker · HTTP REST API · mDNS |
| **Phát hồng ngoại (IR TX)** | Mảng đa hướng 360 độ, bước sóng 940 nm, tầm xa hiệu dụng 8–10 mét |
| **Thu hồng ngoại (IR RX)** | Mắt thu giải mã 38 kHz chuyên dụng, góc nhận 120 độ |
| **Ngõ ra đèn LED** | Hỗ trợ lên đến 5 dải LED WS2812B (RGB) độc lập hoặc LED đơn sắc (PWM) |
| **Ngõ ra Relay & Nút bấm** | 2 tiếp điểm Relay tải cao + 2 chân tín hiệu cảm ứng điện dung |
| **Cảm biến tích hợp** | Cảm biến AHT20: Đo nhiệt độ (±0.3°C) và độ ẩm (±2% RH) |
| **Nhiệt độ hoạt động** | -10°C đến 50°C (Độ ẩm 5% – 90% không ngưng tụ) |
| **Kích thước vỏ hộp** | 82 mm × 82 mm × 45 mm (Vỏ in 3D chống cháy, vật liệu PETG/PLA+) |

### Kênh hỗ trợ khách hàng

- **Trang mã nguồn & Tài liệu dự án:** [github.com/Hdchipeo/NexusIR](https://github.com/Hdchipeo/NexusIR)
- **Hỗ trợ kỹ thuật & Báo lỗi:** [Kênh trao đổi GitHub Issues](https://github.com/Hdchipeo/NexusIR/issues)
- **Email kỹ sư phụ trách:** `tranvanchot73@gmail.com`
- **Địa chỉ hỗ trợ firmware OTA:** Tích hợp trực tiếp tại giao diện Web UI nội bộ thiết bị.

---

## Checklist trước khi phát hành tài liệu

- [x] Đã thay toàn bộ dấu `[ ]`, ảnh mẫu và liên kết ví dụ bằng dữ liệu thực tế của NexusIR.
- [x] Tên nút bấm, mã HomeKit (`111-22-333`), tên mạng SoftAP và thông số kỹ thuật khớp chính xác với firmware codebase v1.4.0.
- [x] Sơ đồ chân phần cứng, các bước cấu hình Wi-Fi và ghép nối HomeKit đã được đối soát qua hình ảnh thực tế.
- [x] Đã bổ sung mục hướng dẫn chuyên sâu học lệnh IR Điều hòa ma trận (AC Matrix 16-30°C) và Quạt (FAN) trên Web Config.
- [x] Mỗi quy trình cài đặt đều có dấu hiệu hoàn tất rõ ràng và hướng dẫn khắc phục sự cố chi tiết khi phát sinh lỗi.
- [x] Cấu trúc tài liệu tuân thủ định dạng chuẩn, sẵn sàng kết xuất sang file PDF chất lượng cao.
