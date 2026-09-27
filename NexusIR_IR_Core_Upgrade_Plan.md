# NexusIR — Kế hoạch nâng cấp toàn diện IR Core

- **Dự án:** [Hdchipeo/NexusIR](https://github.com/Hdchipeo/NexusIR)
- **Ngày lập:** 26/09/2026
- **Phiên bản tài liệu:** 1.0
- **Nền tảng:** ESP32, ESP-IDF, RMT RX/TX
- **Trạng thái:** Kế hoạch đề xuất, chưa triển khai.

> Tài liệu được tổng hợp từ bản phân tích mã nguồn do người dùng cung cấp và kế hoạch đã trao đổi. Chưa đối chiếu thành công với repository hiện tại. Những nhận định về ISR, DMA, định dạng dữ liệu cũ và đường phát Matrix phải được xác minh trên commit triển khai trước khi coi là lỗi đã xác nhận.

## 1. Mục tiêu

1. Học và phát lại lệnh IR ổn định cho TV, quạt và điều hòa.
2. Dùng chung cơ chế kiểm tra, chuẩn hóa, lưu và phát cho Key RAW và AC Matrix.
3. Ngăn ghi đè buffer, tranh chấp trạng thái và lưu frame không đầy đủ.
4. Giữ khả năng sử dụng dữ liệu đã học sau cập nhật firmware.
5. Cung cấp trạng thái và lỗi rõ ràng cho Web UI, HomeKit, RainMaker và ESP-NOW.
6. Tạo kiến trúc dễ mở rộng mà vẫn kiểm soát RAM, flash và thời gian xử lý.

## 2. Nguyên tắc triển khai

- Ưu tiên độ tin cậy của một lần học và phát lại trước khi tái cấu trúc lớn.
- Mỗi đợt thay đổi phải có tiêu chí nghiệm thu và khả năng quay lại bản ổn định.
- Giữ các khoảng nghỉ nội bộ của lệnh AC; không tự động xóa mọi space dài.
- Không coi số lượng symbols đủ lớn là bằng chứng duy nhất của tín hiệu hợp lệ.
- Không mặc định mọi giao thức dùng cùng carrier hoặc cùng cách phát lặp.
- Chọn NVS hay LittleFS dựa trên dung lượng đo được và nhu cầu sử dụng.

## 3. Các nhận định cần xác minh trước khi sửa

- Leading space không tự làm đảo toàn bộ cực tính nếu đường phát giữ nguyên level của từng xung. Cần kiểm tra cách Key RAW và Matrix chuyển dữ liệu sang RMT trước khi kết luận nguyên nhân phát sai.
- `rmt_symbol_word_t` chứa hai đoạn mức logic; không phải symbol nào cũng mặc nhiên là một cặp mark/space hoàn chỉnh. Số đoạn mức logic không đồng nhất tuyệt đối với số cạnh chuyển mức.
- Độ phân giải 1 µs mô tả phép đo đường bao tín hiệu sau mắt thu giải điều chế; không trực tiếp chứng minh sai số đo carrier 38 kHz.
- Ngưỡng kết thúc thu 30 ms cần kiểm tra theo giao thức. Một lệnh có nhiều phần cách nhau bởi khoảng nghỉ dài có thể bị tách thành nhiều frame.
- Hàm gọi từ ISR phải được đối chiếu với ESP-IDF và cấu hình thực tế; tên hàm hoặc việc dùng `esp_rom_printf` không tự bảo đảm toàn bộ callback an toàn và đủ nhanh.
- Dung lượng 600 symbols chưa bảo đảm thu được mọi lệnh AC. Cần đo giao thức mục tiêu và cách driver báo đầy buffer.
- Lỗi hết dung lượng khi ghi NVS cần ghi nhận mã lỗi thực tế, phân biệt với lỗi khởi tạo partition. Không gộp mọi trường hợp thành `ESP_ERR_NVS_NO_FREE_PAGES`.

## 4. P0 — Độ chính xác và an toàn dữ liệu

### IR-01. Hợp nhất đường Key RAW và AC Matrix

- [ ] Cho hai loại lệnh dùng chung pipeline: thu → kiểm tra → chuẩn hóa → lưu → đọc → phát.
- [ ] Tách metadata thiết bị và vị trí Matrix khỏi payload tín hiệu.
- [ ] Loại bỏ các nhánh xử lý RAW trùng lặp.

**Nghiệm thu:** Cùng một capture cho ra cùng waveform khi phát qua Key RAW hoặc Matrix với cùng cấu hình carrier và repeat.

### IR-02. Định dạng bản ghi IR có phiên bản

- [ ] Định nghĩa header gồm magic, version, payload length, pulse/symbol count, đơn vị timing, carrier, cấu hình lặp, flags và CRC.
- [ ] Quy định rõ byte order, độ rộng trường và phạm vi CRC.
- [ ] Serialize từng trường rõ ràng; tránh phụ thuộc padding của struct hoặc ABI bitfield RMT.
- [ ] Kiểm tra kích thước, phép tính tràn số, phiên bản và CRC trước khi cấp phát hoặc phát.

**Nghiệm thu:** Bản ghi thiếu dữ liệu, count sai, CRC sai hoặc version không hỗ trợ đều bị từ chối có mã lỗi rõ ràng.

### IR-03. Chuẩn hóa waveform dùng chung

- [ ] Xử lý theo duration và level thực tế.
- [ ] Bỏ leading space theo quy ước đã định nghĩa, xử lý duration bằng 0 và kiểm tra thứ tự mức logic.
- [ ] Bảo toàn khoảng nghỉ giữa các phần của lệnh.
- [ ] Nếu gộp đoạn cùng mức, xử lý giới hạn duration của RMT khi chuyển trở lại dữ liệu phát.
- [ ] Giữ capture gốc phục vụ chẩn đoán khi cần.

**Nghiệm thu:** Waveform mẫu sau chuẩn hóa giữ đúng thứ tự mark/space và timing cần thiết; không làm mất phần lệnh AC.

### IR-04. Thu gọn RX callback và xác minh ISR

- [ ] Kiểm tra mọi API được gọi trong callback theo phiên bản ESP-IDF mục tiêu.
- [ ] Callback chỉ chuyển sự kiện ngắn sang task IR bằng cơ chế được hỗ trợ trong ISR.
- [ ] Đưa log, chuẩn hóa, thao tác lưu trữ và xử lý LED không phù hợp ISR sang task.
- [ ] Xử lý queue đầy và yêu cầu chuyển lịch đúng theo API sử dụng.

**Nghiệm thu:** Không có thao tác chặn trong ISR; lỗi chuyển sự kiện không làm hệ thống kẹt ở trạng thái học.

### IR-05. Quyền sở hữu buffer và máy trạng thái

- [ ] Task IR quản lý vòng đời buffer RX/TX và trạng thái phiên học.
- [ ] Không thu đè lên capture đang chờ lưu hoặc đang được xử lý.
- [ ] Dùng snapshot bất biến, sao chép có giới hạn hoặc chuyển quyền sở hữu buffer rõ ràng.
- [ ] Thêm session ID để bỏ qua callback hoặc timer thuộc phiên học cũ.
- [ ] Không dùng mutex trực tiếp trong ISR; atomic đơn lẻ cũng không thay thế được quy tắc sở hữu toàn bộ buffer.
- [ ] Quy định xử lý khi phát trong lúc học: từ chối với trạng thái bận hoặc hủy học có kiểm soát.

**Trạng thái đề xuất:** `IDLE`, `ARMED`, `CAPTURED`, `SAVING`, `TRANSMITTING`, `ERROR`, `TIMEOUT`, `CANCELLED`.

**Nghiệm thu:** Start/cancel/save/send liên tiếp từ nhiều nguồn không ghi đè dữ liệu và không sử dụng buffer đã giải phóng. Buffer TX tồn tại đến khi driver xác nhận phát xong.

### IR-06. Chính sách phát lặp theo lệnh

- [ ] Bỏ giả định mọi lệnh đều cần phát hai lần cách 40 ms.
- [ ] Lưu repeat count, khoảng nghỉ và kiểu repeat khi cần.
- [ ] Phân biệt phát lại toàn frame với repeat frame riêng của giao thức.
- [ ] Với lệnh mới chưa xác định hành vi, thử một lần phát rồi xác nhận bằng thiết bị thật.

**Nghiệm thu:** Lệnh nguồn hoặc toggle không bị thực thi hai lần ngoài ý muốn; lệnh cần repeat vẫn hoạt động.

## 5. P1 — Học lệnh ổn định và dễ chẩn đoán

### IR-07. Timeout và hủy phiên học

- [ ] Thêm thời hạn tổng có cấu hình; 15 giây là giá trị khởi đầu đề xuất.
- [ ] Hỗ trợ hủy học, khởi động lại phiên và đưa LED về trạng thái phù hợp.
- [ ] Web UI phân biệt đang chờ, đã nhận, nhiễu, quá dài, hết giờ và đã hủy.

**Nghiệm thu:** Mỗi phiên kết thúc hoặc hết hạn trong thời gian xác định; timer cũ không làm thay đổi phiên mới.

### IR-08. Phát hiện đầy buffer và lệnh nhiều frame

- [ ] Đo số symbols và tổng thời lượng của các remote mục tiêu.
- [ ] Khi capture chạm giới hạn, đánh dấu nghi ngờ bị cắt; đối chiếu cơ chế báo hoàn tất của driver trước khi chấp nhận.
- [ ] Không âm thầm clamp count rồi lưu thành lệnh hợp lệ.
- [ ] Tách khái niệm ngưỡng kết thúc một lượt RMT với thời hạn thu toàn bộ một lệnh.
- [ ] Nếu cần ghép nhiều frame, lưu khoảng nghỉ giữa chúng và giới hạn tổng dung lượng/thời lượng.

**Nghiệm thu:** Lệnh dài hoặc nhiều phần được thu đầy đủ, hoặc bị từ chối rõ ràng thay vì báo thành công giả.

### IR-09. Kiểm tra chất lượng bằng nhiều mẫu

- [ ] Thêm chế độ xác nhận bằng hai hoặc ba lần bấm cùng trạng thái.
- [ ] So sánh cấu trúc và timing trong dung sai; tránh so byte tuyệt đối.
- [ ] Xử lý giao thức có toggle bit hoặc dữ liệu biến đổi giữa các lần bấm.
- [ ] Không tự động lấy trung bình các frame khác cấu trúc.

**Nghiệm thu:** Nhiễu dài không dễ được chấp nhận; remote có toggle bit không bị loại bỏ một cách máy móc.

### IR-10. Log và metadata chẩn đoán

- [ ] Ghi session ID, số symbols, thời lượng, lý do từ chối, lỗi driver và kết quả lưu/phát.
- [ ] Có chế độ debug xem hoặc xuất capture gốc và dữ liệu đã chuẩn hóa.
- [ ] Giới hạn log để tránh ảnh hưởng đường xử lý thời gian thực.

**Nghiệm thu:** Một lần học thất bại có thông tin đủ để phân biệt lỗi thu, dữ liệu, lưu trữ và phát.

### IR-11. Xác minh phần cứng

- [ ] Đo cực tính RX, timing đường bao và nhiễu khi bật Wi-Fi hoặc phát IR.
- [ ] Chọn pull-up theo datasheet và mạch mắt thu cụ thể.
- [ ] Kiểm tra nguồn mắt thu, bố trí dây và hiện tượng tự thu tín hiệu TX.
- [ ] Nếu cần bù sai lệch mark/space, xác định bằng số đo; không áp dụng hằng số bù cho mọi mắt thu.

**Nghiệm thu:** Có capture tham chiếu và kết quả thử ở khoảng cách, góc và môi trường ánh sáng được ghi rõ.

## 6. P2 — Lưu trữ, API và tương thích

### IR-12. Đo ngân sách lưu trữ và chọn backend

- [ ] Đo kích thước bản ghi TV, quạt, AC và tổng số lệnh dự kiến.
- [ ] Kiểm tra partition thực tế, overhead và dung lượng dự phòng.
- [ ] Giữ NVS khi số lượng lệnh nhỏ và phù hợp ngân sách.
- [ ] Cân nhắc LittleFS khi cần nhiều payload, backup/restore hoặc quản lý tệp lớn.
- [ ] Tách API lưu trữ khỏi backend để có thể đổi sau.

**Nghiệm thu:** Có bảng dung lượng và xử lý hết chỗ không làm mất các lệnh đã lưu.

### IR-13. Ghi an toàn và di trú dữ liệu

- [ ] Đọc định dạng Key RAW và Matrix cũ theo quy tắc riêng đã xác minh.
- [ ] Viết bản mới, kiểm tra rồi mới cập nhật tham chiếu sử dụng theo khả năng của backend.
- [ ] Giữ đường khôi phục khi mất điện giữa các bước.
- [ ] Không xóa bản cũ trước khi xác nhận di trú thành công.
- [ ] Ghi rõ khả năng đọc dữ liệu khi rollback firmware; backup trước chuyển đổi phá vỡ tương thích.

**Nghiệm thu:** Dữ liệu cũ còn sử dụng được; ngắt nguồn trong quá trình chuyển đổi không làm mất cả bản cũ lẫn bản mới.

### IR-14. API dịch vụ thống nhất

- [ ] Cung cấp `start_learn`, `cancel_learn`, `get_status`, `save`, `list`, `send`, `delete`.
- [ ] Các bên gọi dùng cùng mã lỗi, session ID và quy tắc bận.
- [ ] Giới hạn ID, index Matrix, kích thước yêu cầu và số lệnh đang chờ.
- [ ] Thêm import/export sau khi định dạng bản ghi ổn định; kiểm tra dữ liệu nhập trước khi ghi.

**Nghiệm thu:** Web UI, HomeKit, RainMaker và ESP-NOW tuân theo cùng chính sách thu/phát/lưu.

### IR-15. Đồng bộ tài liệu

- [ ] Cập nhật README về nơi lưu dữ liệu thực tế.
- [ ] Mô tả định dạng, giới hạn, cấu hình Kconfig và quy trình di trú.
- [ ] Bổ sung hướng dẫn học đúng, thử phát, xử lý lỗi và phục hồi dữ liệu.

**Nghiệm thu:** Tài liệu khớp với phiên bản firmware phát hành.

## 7. P3 — Kiến trúc và mở rộng

### IR-16. Đưa RX/TX về driver RMT

- [ ] Driver sở hữu channel, GPIO, DMA nếu hỗ trợ, encoder và sự kiện hoàn tất.
- [ ] Manager sở hữu phiên học, chất lượng dữ liệu và chính sách phát.
- [ ] Storage sở hữu serialize, kiểm tra bản ghi và backend.
- [ ] Cấu hình theo khả năng chip; không sao chép nguyên cấu hình DMA hoặc bộ nhớ giữa S3 và C3.

**Nghiệm thu:** Manager không cần trực tiếp khởi tạo channel RMT; build được trên từng target thực sự hỗ trợ.

### IR-17. Carrier cấu hình theo thiết bị hoặc lệnh

- [ ] Giữ giá trị mặc định có cấu hình và cho phép chỉnh khi có căn cứ.
- [ ] Kiểm tra giới hạn tần số, duty cycle và cực tính phát.
- [ ] Phân biệt carrier do người dùng đặt, do protocol xác định và giá trị mặc định.

**Lưu ý:** Mắt thu giải điều chế thông thường không cung cấp phép đo đáng tin cậy để tự suy ra carrier của remote.

**Nghiệm thu:** Carrier cấu hình được áp dụng đúng và xác minh bằng phép đo phù hợp.

### IR-18. Tách hàm xử lý dữ liệu để kiểm thử

- [ ] Tách validate, normalize, serialize và deserialize khỏi ESP-IDF/RMT.
- [ ] Dùng capture thực tế làm bộ mẫu hồi quy.
- [ ] Kiểm thử dữ liệu hỏng, count biên, duration dài và frame nhiều phần.

**Nghiệm thu:** Phát hiện hồi quy xử lý dữ liệu trên máy phát triển mà không cần phần cứng cho mọi ca kiểm thử.

### IR-19. Giải mã protocol tùy chọn

- [ ] Chỉ thêm decoder/encoder khi có lợi ích rõ: lưu gọn, repeat chuẩn hoặc điều khiển trạng thái AC.
- [ ] Duy trì RAW làm đường xử lý cho remote chưa được hỗ trợ.
- [ ] Công bố giao thức được kiểm thử và mức hỗ trợ thực tế.

**Nghiệm thu:** Thêm giao thức mới không làm thay đổi hành vi RAW đã nghiệm thu.

## 8. Kiến trúc mục tiêu

| Thành phần | Trách nhiệm |
| --- | --- |
| Web UI / HomeKit / RainMaker / ESP-NOW | Gửi yêu cầu và hiển thị trạng thái |
| Dịch vụ IR | Quản lý phiên, điều phối học/phát/lưu, xử lý yêu cầu bận |
| Xử lý waveform | Kiểm tra và chuẩn hóa dữ liệu, phát hiện frame không đầy đủ |
| Driver RMT | Thu/phát, cấu hình phần cứng, thông báo hoàn tất |
| Storage | Định dạng có version, CRC, di trú và backend |
| AC Matrix | Ánh xạ trạng thái thiết bị tới bản ghi IR |

Tên module và API cụ thể được chốt sau khi rà soát dependency trên commit triển khai.

## 9. Lộ trình triển khai

| Đợt | Hạng mục | Điều kiện hoàn thành |
| --- | --- | --- |
| 1 | Chốt commit, capture mẫu; IR-01, IR-03, IR-05, IR-06, phần phát hiện cắt cụt của IR-08 | Key RAW/Matrix dùng chung dữ liệu, không ghi đè capture, không phát lặp ngoài ý muốn |
| 2 | IR-04, IR-07 đến IR-11 | Học/hủy/hết giờ nhiều lần không treo; có trạng thái lỗi rõ ràng |
| 3 | IR-02, IR-13, IR-14 | Bản ghi có kiểm tra toàn vẹn; dữ liệu cũ có đường chuyển đổi |
| 4 | IR-12, IR-15, IR-16 | Chọn backend dựa trên số đo; tách driver và cập nhật tài liệu |
| 5 | IR-17, IR-18, IR-19 khi cần; kiểm thử hồi quy toàn bộ | Đạt ma trận nghiệm thu trên remote và target đã công bố |

Các kiểm thử đơn vị cần thiết được bổ sung ngay khi tách hàm xử lý ở những đợt đầu; không đợi tới đợt 5 mới kiểm tra dữ liệu.

## 10. Ma trận kiểm thử nghiệm thu

| Nhóm | Tình huống | Kết quả mong đợi |
| --- | --- | --- |
| Học cơ bản | TV, quạt, AC; nhiều lần bấm | Capture hợp lệ, phát lại đúng chức năng |
| Đồng nhất | Cùng capture qua RAW và Matrix | Waveform tương đương khi cấu hình phát giống nhau |
| Toggle/repeat | Nguồn, chuyển chế độ, giữ phím | Không lặp thao tác ngoài ý muốn |
| Lệnh dài | Chạm giới hạn buffer, AC nhiều phần | Thu đầy đủ hoặc báo quá dài/nghi cắt cụt |
| Nhiễu | Ánh sáng, nhiễu nguồn, không có remote | Không lưu rác dễ dàng; kết thúc theo timeout |
| Đồng thời | Start/cancel/save/send liên tiếp | Không race, không ghi đè, trả trạng thái bận đúng |
| Timer cũ | Hủy và bắt đầu phiên mới nhanh | Sự kiện phiên cũ không tác động phiên mới |
| Lưu trữ | Đầy bộ nhớ, blob sai count/CRC/version | Từ chối rõ ràng, không ảnh hưởng lệnh khác |
| Mất nguồn | Sau lưu và giữa quá trình ghi/di trú | Phục hồi theo thiết kế, không mất cả bản dự phòng |
| Reboot | Phát lại lệnh sau khởi động | Đọc và phát đúng cấu hình |
| Tài nguyên | Lặp chu trình học/lưu/phát | Không tăng bộ nhớ sử dụng bất thường hoặc cạn tài nguyên |
| Phần cứng | Từng target được hỗ trợ | Build và vận hành đúng cấu hình RMT |

### Ghi nhận số đo

- Commit firmware, phiên bản ESP-IDF, target và cấu hình build.
- Model remote, thiết bị đích, mắt thu và mạch phát.
- Số lần thử, tỷ lệ thiết bị nhận lệnh, khoảng cách và góc thử.
- Số symbols, thời lượng capture, mức sử dụng RAM và dung lượng bản ghi.
- Sai lệch timing RX/TX sau khi đo tại các điểm có thể so sánh được.

Không áp dụng một ngưỡng sai lệch timing duy nhất cho mọi protocol khi chưa đo và xác định yêu cầu của thiết bị đích.

## 11. Danh sách bàn giao

- [ ] Driver RX/TX và dịch vụ IR đã tách trách nhiệm.
- [ ] Pipeline RAW/Matrix thống nhất.
- [ ] Máy trạng thái, session ID, timeout và hủy học.
- [ ] Định dạng bản ghi có version và CRC.
- [ ] Công cụ hoặc cơ chế di trú dữ liệu cũ.
- [ ] API và mã lỗi thống nhất cho các giao diện.
- [ ] Capture mẫu và kiểm thử hồi quy.
- [ ] Báo cáo dung lượng, timing và kết quả thử thiết bị thật.
- [ ] README, hướng dẫn cấu hình, backup/restore và rollback.

## 12. Tiếng anh

| Tiếng anh | Nghĩa tiếng Việt |
| --- | --- |
| Capture | Thu tín hiệu |
| Waveform | Dạng sóng |
| Mark / Space | Khoảng có / không có sóng mang IR |
| Normalize | Chuẩn hóa dữ liệu |
| Replay | Phát lại tín hiệu đã học |
| Carrier | Sóng mang |
| Repeat frame | Khung lặp của giao thức |
| Buffer ownership | Quyền sở hữu và quản lý bộ đệm |
| State machine | Máy trạng thái |
| Race condition | Tranh chấp do thứ tự truy cập đồng thời |
| Truncated frame | Khung bị cắt cụt |
| Serialization | Chuyển dữ liệu sang định dạng lưu/truyền |
| Migration | Chuyển đổi dữ liệu từ định dạng cũ |
| Regression test | Kiểm thử phát hiện lỗi tái xuất hiện |
| Acceptance criteria | Tiêu chí nghiệm thu |
| Rollback | Quay lại phiên bản trước |

## 13. Từ khóa tra cứu khi triển khai

- `ESP-IDF RMT receive callback ISR safety`
- `ESP-IDF RMT RX partial reception`
- `ESP-IDF RMT transmit buffer lifetime`
- `ESP-IDF NVS blob storage`
- `FreeRTOS queue from ISR`
- `IR protocol repeat frame toggle bit`
- `IR demodulator mark space distortion`

Cần đối chiếu tài liệu Espressif đúng phiên bản ESP-IDF và target sử dụng trước khi chốt API hoặc cấu hình phần cứng.
