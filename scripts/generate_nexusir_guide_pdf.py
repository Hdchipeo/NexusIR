import os
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak
)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

# System TrueType Fonts trên macOS hỗ trợ đầy đủ tiếng Việt Unicode
FONT_REGULAR = "/System/Library/Fonts/Supplemental/Arial.ttf"
FONT_BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
FONT_ITALIC = "/System/Library/Fonts/Supplemental/Arial Italic.ttf"

pdfmetrics.registerFont(TTFont('Arial', FONT_REGULAR))
pdfmetrics.registerFont(TTFont('Arial-Bold', FONT_BOLD))
pdfmetrics.registerFont(TTFont('Arial-Italic', FONT_ITALIC))

# Bảng màu tối giản phong cách Apple
COLOR_TEXT_MAIN = colors.HexColor("#1D1D1F")
COLOR_TEXT_MUTED = colors.HexColor("#6E6E73")
COLOR_ACCENT = colors.HexColor("#0071E3")
COLOR_BG_CARD = colors.HexColor("#F5F5F7")
COLOR_BORDER = colors.HexColor("#E5E5EA")

class AppleNumberedCanvas(canvas.Canvas):
    """
    Two-pass canvas để tự động tính toán tổng số trang và in Header/Footer chuẩn Apple.
    """
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_page_decorations(num_pages)
            super().showPage()
        super().save()

    def draw_page_decorations(self, total_pages):
        self.saveState()
        self.setFont("Arial", 8.5)
        self.setFillColor(COLOR_TEXT_MUTED)
        
        # Header (từ trang 2 trở đi)
        if self._pageNumber > 1:
            self.drawString(50, 804, "NexusIR Smart Hub  ·  Hướng Dẫn Sử Dụng")
            self.setStrokeColor(COLOR_BORDER)
            self.setLineWidth(0.6)
            self.line(50, 796, 545, 796)
            
        # Footer (toàn bộ các trang)
        page_text = f"Trang {self._pageNumber} / {total_pages}"
        self.drawRightString(545, 36, page_text)
        self.drawString(50, 36, "NexusIR  ·  Tài liệu kỹ thuật và hướng dẫn vận hành")
        self.setStrokeColor(COLOR_BORDER)
        self.setLineWidth(0.6)
        self.line(50, 48, 545, 48)
        
        self.restoreState()


def build_styles():
    styles = getSampleStyleSheet()
    
    title_style = ParagraphStyle(
        'DocTitle',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=24,
        leading=28,
        textColor=COLOR_TEXT_MAIN,
        alignment=0,
        spaceAfter=6
    )
    
    subtitle_style = ParagraphStyle(
        'DocSubtitle',
        parent=styles['Normal'],
        fontName='Arial',
        fontSize=11.5,
        leading=16,
        textColor=COLOR_TEXT_MUTED,
        spaceAfter=12
    )
    
    h1_style = ParagraphStyle(
        'Heading1',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=13.5,
        leading=17.5,
        textColor=COLOR_TEXT_MAIN,
        spaceBefore=10,
        spaceAfter=6,
        keepWithNext=True
    )
    
    h2_style = ParagraphStyle(
        'Heading2',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=10,
        leading=13.5,
        textColor=COLOR_TEXT_MAIN,
        spaceBefore=6,
        spaceAfter=3,
        keepWithNext=True
    )

    body_style = ParagraphStyle(
        'BodyText',
        parent=styles['Normal'],
        fontName='Arial',
        fontSize=9,
        leading=13.2,
        textColor=COLOR_TEXT_MAIN,
        spaceAfter=4.5
    )
    
    body_bold_style = ParagraphStyle(
        'BodyBold',
        parent=body_style,
        fontName='Arial-Bold'
    )
    
    bullet_style = ParagraphStyle(
        'BulletText',
        parent=body_style,
        leftIndent=11,
        firstLineIndent=-7,
        spaceAfter=3.5
    )

    callout_style = ParagraphStyle(
        'CalloutText',
        parent=body_style,
        fontName='Arial',
        fontSize=8.6,
        leading=12.5,
        textColor=COLOR_TEXT_MAIN,
        backColor=COLOR_BG_CARD,
        borderColor=COLOR_ACCENT,
        borderWidth=0.8,
        borderPadding=7,
        spaceBefore=6,
        spaceAfter=7,
        borderRadius=4
    )

    badge_style = ParagraphStyle(
        'BadgeText',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=8,
        leading=10,
        textColor=COLOR_ACCENT
    )

    return {
        'title': title_style,
        'subtitle': subtitle_style,
        'h1': h1_style,
        'h2': h2_style,
        'body': body_style,
        'body_bold': body_bold_style,
        'bullet': bullet_style,
        'callout': callout_style,
        'badge': badge_style,
    }


def render_step_card(step_no, title, text, img_path, st):
    img = Image(img_path, width=95, height=205)
    content = [
        Paragraph(f"<b>Bước {step_no}: {title}</b>", st['h2']),
        Paragraph(text, st['body']),
    ]
    t = Table([[content, img]], colWidths=[380, 115])
    t.setStyle(TableStyle([
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
        ('ALIGN', (1,0), (1,0), 'CENTER'),
        ('BACKGROUND', (0,0), (-1,-1), colors.white),
        ('BOX', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('TOPPADDING', (0,0), (-1,-1), 8),
        ('BOTTOMPADDING', (0,0), (-1,-1), 8),
        ('LEFTPADDING', (0,0), (-1,-1), 10),
        ('RIGHTPADDING', (0,0), (-1,-1), 10),
    ]))
    return t


def create_nexusir_pdf():
    pdf_path = "docs/Huong_Dan_Su_Dung_NexusIR.pdf"
    os.makedirs(os.path.dirname(pdf_path), exist_ok=True)
    
    # A4: 595.27 x 841.89 pt. Content width: 495.27 pt. Content height: 734.89 pt
    doc = SimpleDocTemplate(
        pdf_path,
        pagesize=A4,
        leftMargin=50,
        rightMargin=50,
        topMargin=52,
        bottomMargin=55
    )
    
    st = build_styles()
    story = []

    # ====================================================
    # TRANG 1: BÌA & KHỞI ĐỘNG NHANH TRONG 5 PHÚT
    # ====================================================
    story.append(Paragraph("HƯỚNG DẪN SỬ DỤNG", st['badge']))
    story.append(Spacer(1, 3))
    story.append(Paragraph("NexusIR Smart Hub", st['title']))
    story.append(Paragraph("Trung tâm điều khiển hồng ngoại và ánh sáng thông minh cho Apple HomeKit", st['subtitle']))
    
    if os.path.exists("hardware/3d_print/white.png"):
        hero_img = Image("hardware/3d_print/white.png", width=460, height=164)
        story.append(hero_img)
        story.append(Spacer(1, 8))
    
    meta_data = [
        [
            Paragraph("<b>Phiên bản tài liệu:</b> 1.1", st['body']),
            Paragraph("<b>Cập nhật:</b> 23/09/2026", st['body']),
            Paragraph("<b>Áp dụng:</b> Firmware v1.4.0+", st['body']),
        ]
    ]
    t_meta = Table(meta_data, colWidths=[150, 150, 195])
    t_meta.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), COLOR_BG_CARD),
        ('ROUNDEDCORNERS', [4, 4, 4, 4]),
        ('TOPPADDING', (0,0), (-1,-1), 5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 5),
        ('LEFTPADDING', (0,0), (-1,-1), 10),
        ('RIGHTPADDING', (0,0), (-1,-1), 10),
    ]))
    story.append(t_meta)
    story.append(Spacer(1, 10))
    
    story.append(Paragraph("1. Bắt Đầu Trong 5 Phút", st['h1']))
    story.append(Paragraph("Thiết lập nhanh để sẵn sàng điều khiển toàn bộ điều hòa, quạt và dải đèn LED chỉ với 4 thao tác:", st['body']))
    
    quick_steps = [
        Paragraph("<b>1. Cấp nguồn:</b> Cắm cáp Type-C (5V/2A) vào thiết bị. Đèn LED trên hub sẽ sáng nháy vàng chậm.", st['bullet']),
        Paragraph("<b>2. Kết nối Wi-Fi thiết bị:</b> Trên iPhone, vào <i>Cài đặt -> Wi-Fi</i>, chọn mạng <b>NexusIR-Setup-XXXX</b> (hoặc <i>GhostMagic-Setup-XXXX</i>).", st['bullet']),
        Paragraph("<b>3. Cấu hình mạng:</b> Trang xác thực tự hiện ra. Chọn Wi-Fi nhà bạn (băng tần 2.4GHz), nhập mật khẩu rồi bấm <b>Join</b>.", st['bullet']),
        Paragraph("<b>4. Thêm vào Apple Home:</b> Mở ứng dụng <b>Nhà (Home)</b>, bấm <b>(+) -> Thêm phụ kiện</b>, chọn <i>NexusIR Bridge</i> và nhập mã: <b>111-22-333</b>.", st['bullet']),
    ]
    for qs in quick_steps:
        story.append(qs)
        
    story.append(Spacer(1, 4))
    story.append(Paragraph("<b>Dấu hiệu hoàn tất:</b> Đèn báo trên hub chuyển sang màu xanh dương ổn định. Biểu tượng cầu nối NexusIR xuất hiện trên màn hình chính của ứng dụng Nhà.", st['callout']))

    story.append(PageBreak())

    # ====================================================
    # TRANG 2: TRƯỚC KHI SỬ DỤNG & LÀM QUEN PHẦN CỨNG
    # ====================================================
    story.append(Paragraph("2. Trước Khi Sử Dụng", st['h1']))
    
    pre_info = [
        [
            Paragraph("<b>Trong hộp sản phẩm</b>", st['h2']),
            Paragraph("<b>Yêu cầu cần chuẩn bị</b>", st['h2'])
        ],
        [
            Paragraph("• 1× Thiết bị NexusIR Smart Hub<br/>• 1× Cáp nguồn USB Type-C (1.0m)<br/>• 1× Thẻ mã thiết lập HomeKit Setup Code<br/>• 1× Miếng dán định vị 3M chuyên dụng", st['body']),
            Paragraph("• Củ sạc USB 5V (tối thiểu 1.5A – 2A)<br/>• Mạng Wi-Fi <b>2.4 GHz</b> (802.11 b/g/n)<br/>• iPhone/iPad chạy iOS 15.0 trở lên<br/>• Bật sẵn Bluetooth và Định vị trên iPhone", st['body'])
        ]
    ]
    t_pre = Table(pre_info, colWidths=[240, 255])
    t_pre.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), COLOR_BG_CARD),
        ('GRID', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('TOPPADDING', (0,0), (-1,-1), 5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 5),
        ('LEFTPADDING', (0,0), (-1,-1), 8),
        ('RIGHTPADDING', (0,0), (-1,-1), 8),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
    ]))
    story.append(t_pre)
    
    story.append(Spacer(1, 4))
    story.append(Paragraph("<b>Lưu ý quan trọng:</b> Thiết bị chỉ hoạt động trên băng tần Wi-Fi <b>2.4 GHz</b>. Nếu bộ định tuyến nhà bạn dùng tính năng gộp băng tần (Mesh/Smart Connect), hãy tạm thời tách riêng SSID 2.4 GHz hoặc đứng gần router trong quá trình thiết lập ban đầu.", st['callout']))
    
    story.append(Spacer(1, 8))
    story.append(Paragraph("3. Làm Quen Với Sản Phẩm", st['h1']))
    story.append(Paragraph("NexusIR sở hữu thiết kế hình khối vòm khí động học, phát tín hiệu hồng ngoại 360 độ góc rộng.", st['body']))
    
    hw_table_data = [
        [Paragraph("<b>Số</b>", st['body_bold']), Paragraph("<b>Bộ phận phần cứng</b>", st['body_bold']), Paragraph("<b>Tác dụng & Ý nghĩa vận hành</b>", st['body_bold'])],
        [Paragraph("1", st['body']), Paragraph("Vòm phát IR 360°", st['body_bold']), Paragraph("Mảng LED hồng ngoại 940nm điều khiển điều hòa, quạt, TV từ mọi góc.", st['body'])],
        [Paragraph("2", st['body']), Paragraph("Mắt thu hồng ngoại", st['body_bold']), Paragraph("Cảm biến giải mã 38 kHz dùng để sao chép / học lệnh remote gốc.", st['body'])],
        [Paragraph("3", st['body']), Paragraph("Cảm biến AHT20", st['body_bold']), Paragraph("Đo nhiệt độ và độ ẩm phòng theo thời gian thực (chuẩn giao tiếp I2C).", st['body'])],
        [Paragraph("4", st['body']), Paragraph("Ngõ ra LED & Relay", st['body_bold']), Paragraph("Hỗ trợ tối đa 5 dải đèn LED RGB WS2812B và 2 ngõ ra relay tải cao.", st['body'])],
        [Paragraph("5", st['body']), Paragraph("Nút nhấn đa năng", st['body_bold']), Paragraph("Bấm 1 lần để đổi hiệu ứng đèn; Giữ 5 giây để xóa Wi-Fi và vào Setup Mode.", st['body'])],
        [Paragraph("6", st['body']), Paragraph("Cổng nguồn Type-C", st['body_bold']), Paragraph("Cấp nguồn DC 5V tiêu chuẩn cho toàn bộ hệ thống.", st['body'])],
    ]
    t_hw = Table(hw_table_data, colWidths=[25, 130, 340])
    t_hw.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), COLOR_BG_CARD),
        ('GRID', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('TOPPADDING', (0,0), (-1,-1), 4),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t_hw)
    
    story.append(Spacer(1, 8))
    story.append(Paragraph("Trạng thái đèn LED chỉ báo", st['h2']))
    
    led_table_data = [
        [Paragraph("<b>Màu sắc & Nhịp điệu</b>", st['body_bold']), Paragraph("<b>Ý nghĩa trạng thái</b>", st['body_bold']), Paragraph("<b>Hành động yêu cầu</b>", st['body_bold'])],
        [Paragraph("Nhấp nháy vàng chậm", st['body']), Paragraph("Đang phát Wi-Fi cài đặt (SoftAP)", st['body']), Paragraph("Mở Wi-Fi điện thoại kết nối vào trạm phát", st['body'])],
        [Paragraph("Nhấp nháy xanh lá nhanh", st['body']), Paragraph("Đang kết nối vào Wi-Fi nhà", st['body']), Paragraph("Đợi 5–10 giây thiết bị tự xác thực", st['body'])],
        [Paragraph("Xanh dương sáng tĩnh", st['body']), Paragraph("Đã kết nối và đồng bộ HomeKit", st['body']), Paragraph("Sẵn sàng sử dụng bình thường", st['body'])],
        [Paragraph("Nháy đỏ 2 lần", st['body']), Paragraph("Lỗi mật khẩu Wi-Fi hoặc mất mạng", st['body']), Paragraph("Nhấn giữ nút 5s để cài đặt lại Wi-Fi", st['body'])],
    ]
    t_led = Table(led_table_data, colWidths=[130, 175, 190])
    t_led.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), COLOR_BG_CARD),
        ('GRID', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('TOPPADDING', (0,0), (-1,-1), 3.5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3.5),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t_led)

    story.append(PageBreak())

    # ====================================================
    # TRANG 3: THIẾT LẬP WI-FI (BƯỚC 1 & 2)
    # ====================================================
    story.append(Paragraph("4. Thiết Lập Lần Đầu", st['h1']))
    story.append(Paragraph("Phần I: Cấu Hình Mạng Wi-Fi Cho Thiết Bị (Wi-Fi Provisioning)", st['h2']))
    story.append(Paragraph("Thiết bị tự động phát mạng Wi-Fi cục bộ để nhận thông tin định tuyến mạng gia đình của bạn.", st['body']))
    story.append(Spacer(1, 4))
    
    t1 = render_step_card(
        1, "Tìm và chọn mạng Wi-Fi thiết bị",
        "Cấp nguồn cho NexusIR bằng củ sạc USB. Trên iPhone, mở ứng dụng <b>Cài đặt -> Wi-Fi</b>.<br/><br/>"
        "Trong danh sách mạng tìm kiếm được, chọn mạng Wi-Fi do thiết bị phát ra có tên: <b>NexusIR-Setup-XXXX</b> (hoặc <i>GhostMagic-Setup-598D</i> như trong hình).",
        "docs/images/step1_annotated.png",
        st
    )
    story.append(t1)
    story.append(Spacer(1, 10))

    t2 = render_step_card(
        2, "Xác nhận kết nối thành công",
        "Chờ iPhone kết nối với mạng Wi-Fi của thiết bị cho đến khi xuất hiện <b>dấu tích xanh</b>.<br/><br/>"
        "<i>Lưu ý:</i> Dưới tên Wi-Fi hiển thị 'Mạng không bảo mật' hoặc không có internet là hoàn toàn bình thường vì đây chỉ là cổng kết nối cấu hình tạm thời.",
        "docs/images/step2_annotated.png",
        st
    )
    story.append(t2)

    story.append(PageBreak())

    # ====================================================
    # TRANG 4: THIẾT LẬP WI-FI (BƯỚC 3 & 4)
    # ====================================================
    story.append(Paragraph("Cấu Hình Wi-Fi (Tiếp Theo)", st['h1']))
    story.append(Paragraph("Nhập thông tin xác thực để kết nối thiết bị vào bộ định tuyến nhà bạn.", st['body']))
    story.append(Spacer(1, 4))

    t3 = render_step_card(
        3, "Chọn mạng Wi-Fi gia đình",
        "Màn hình đăng nhập <b>Captive Portal (Wi-Fi xác thực)</b> sẽ tự động hiển thị trên điện thoại.<br/><br/>"
        "<i>(Nếu không tự bật, mở trình duyệt Safari và gõ địa chỉ IP: <b>192.168.4.1</b>).</i><br/><br/>"
        "Chạm chọn đúng <b>tên Wi-Fi nhà bạn</b> (băng tần 2.4GHz, ví dụ <i>DEVICE_2.4G</i>).",
        "docs/images/step3_annotated.png",
        st
    )
    story.append(t3)
    story.append(Spacer(1, 10))

    t4 = render_step_card(
        4, "Nhập mật khẩu Wi-Fi và lưu cấu hình",
        "Hộp thoại bảo mật hiện ra, hãy nhập chính xác mật khẩu Wi-Fi nhà bạn vào ô nhập (<b>4a</b>).<br/><br/>"
        "Sau đó chạm vào nút <b>Join</b> (<b>4b</b>) ở góc dưới bên phải để thiết bị ghi thông tin vào bộ nhớ Flash NVS.",
        "docs/images/step4_annotated.png",
        st
    )
    story.append(t4)

    story.append(PageBreak())

    # ====================================================
    # TRANG 5: THIẾT LẬP WI-FI (BƯỚC 5) & HOMEKIT (BƯỚC 6)
    # ====================================================
    story.append(Paragraph("Hoàn Tất Wi-Fi & Bắt Đầu Ghép Nối HomeKit", st['h1']))
    story.append(Paragraph("Lưu cấu hình Wi-Fi và chuẩn bị đưa thiết bị vào ứng dụng Apple Home.", st['body']))
    story.append(Spacer(1, 4))

    t5 = render_step_card(
        5, "Khởi động lại và lưu mạng",
        "Màn hình xuất hiện thông báo nổi màu đen ở chân trang: <b>Success! Rebooting...</b> báo hiệu lưu cấu hình thành công.<br/><br/>"
        "NexusIR sẽ tự khởi động lại và tự động kết nối vào mạng Wi-Fi gia đình bạn. Lúc này điện thoại iPhone cũng tự kết nối trở lại mạng nhà.",
        "docs/images/step5_annotated.png",
        st
    )
    story.append(t5)
    story.append(Spacer(1, 10))

    story.append(Paragraph("Phần II: Đồng Bộ Với Apple HomeKit (HomeKit Pairing)", st['h2']))
    story.append(Paragraph("Đưa phụ kiện cầu nối NexusIR vào ứng dụng Nhà trên hệ điều hành iOS.", st['body']))
    story.append(Spacer(1, 4))

    t6 = render_step_card(
        6, "Mở ứng dụng Nhà và thêm phụ kiện",
        "Đảm bảo iPhone đang kết nối cùng mạng Wi-Fi 2.4GHz với hub.<br/><br/>"
        "Mở ứng dụng <b>Home (Nhà)</b>, chạm vào biểu tượng dấu <b>(+)</b> ở góc trên cùng bên phải và chọn <b>Thêm Phụ Kiện (Add Accessory)</b>.",
        "docs/images/step6_annotated.png",
        st
    )
    story.append(t6)

    story.append(PageBreak())

    # ====================================================
    # TRANG 6: HOMEKIT PAIRING (BƯỚC 7 & 8)
    # ====================================================
    story.append(Paragraph("Đồng Bộ Apple HomeKit (Tiếp Theo)", st['h1']))
    story.append(Paragraph("Nhập mã xác thực bảo mật và chấp thuận phụ kiện.", st['body']))
    story.append(Spacer(1, 4))

    t7 = render_step_card(
        7, "Nhập mã thiết lập 8 số HomeKit",
        "Tại giao diện quét mã, chọn dòng chữ <i>'Tôi không có mã hoặc không thể quét'</i> hoặc <i>'Thêm bằng mã...'</i>.<br/><br/>"
        "Chọn <b>NexusIR Bridge</b> (hoặc <i>GhostLamp Bridge</i>) trong danh sách phụ kiện lân cận.<br/><br/>"
        "Nhập mã thiết lập 8 chữ số: <b>111-22-333</b> (nhập liền <b>11122333</b>).",
        "docs/images/step7_annotated.png",
        st
    )
    story.append(t7)
    story.append(Spacer(1, 10))

    t8 = render_step_card(
        8, "Xác nhận phụ kiện",
        "Khi xuất hiện thông báo cảnh báo phụ kiện chưa được chứng nhận từ Apple.<br/><br/>"
        "Hãy chọn nút <b>Vẫn thêm (Add Anyway)</b> ở phía bên trái để tiếp tục quá trình tích hợp cầu nối.",
        "docs/images/step8_annotated.png",
        st
    )
    story.append(t8)

    story.append(PageBreak())

    # ====================================================
    # TRANG 7: HOMEKIT PAIRING (BƯỚC 9 & 10)
    # ====================================================
    story.append(Paragraph("Cấu Hình Phụ Kiện Apple Home", st['h1']))
    story.append(Paragraph("Thiết lập mã hóa kết nối và gán phòng cho từng thiết bị ngoại vi.", st['body']))
    story.append(Spacer(1, 4))

    t9 = render_step_card(
        9, "Chờ thêm vào màn hình chính",
        "Hệ thống sẽ tiến hành đàm phán bảo mật và thiết lập kênh mã hóa với cầu nối NexusIR.<br/><br/>"
        "Khi màn hình báo thêm thành công, chọn <b>Tiếp tục (Continue)</b> ở góc dưới màn hình.",
        "docs/images/step9_annotated.png",
        st
    )
    story.append(t9)
    story.append(Spacer(1, 10))

    t10 = render_step_card(
        10, "Đặt tên và phân bổ phòng",
        "Chọn phòng đặt thiết bị (ví dụ: <i>Phòng khách, Phòng ngủ</i>). Đặt tên gợi nhớ cho cầu nối và các thiết bị liên kết.<br/><br/>"
        "Bấm <b>Tiếp tục</b> để xác nhận từng phụ kiện (Điều hòa, Đèn LED, Cảm biến, Quạt).",
        "docs/images/step10_annotated.png",
        st
    )
    story.append(t10)

    story.append(PageBreak())

    # ====================================================
    # TRANG 8: HOÀN TẤT HOMEKIT (BƯỚC 11) & GIAO DIỆN KẾT QUẢ
    # ====================================================
    story.append(Paragraph("Hoàn Tất Thiết Lập & Bảng Điều Khiển", st['h1']))
    story.append(Spacer(1, 4))

    t11 = render_step_card(
        11, "Hoàn tất cài đặt",
        "Màn hình thông báo phụ kiện đã được thêm thành công vào Nhà.<br/><br/>"
        "Chạm vào nút <b>Xong (Done)</b> ở góc dưới cùng để bắt đầu trải nghiệm điều khiển.",
        "docs/images/step11_annotated.png",
        st
    )
    story.append(t11)
    story.append(Spacer(1, 10))

    story.append(Paragraph("Giao Diện Hoạt Động Thực Tế Trên Apple Home", st['h2']))
    
    img_res1 = Image("docs/images/result1.jpg", width=155, height=147)
    img_res2 = Image("docs/images/result2.png", width=72, height=155)
    
    res_desc = [
        Paragraph("<b>Trải nghiệm sau khi thêm thành công:</b>", st['body_bold']),
        Paragraph("• Hiển thị đầy đủ Điều hòa, Đèn thông minh, Cảm biến nhiệt ẩm và Quạt trong ứng dụng Apple Home.", st['bullet']),
        Paragraph("• Bật/tắt nhanh, điều chỉnh độ sáng và chọn màu sắc trong bảng 16 triệu màu RGB.", st['bullet']),
        Paragraph("• Tích hợp nút <b>Web Config</b> để mở nhanh dashboard quản trị nội bộ.", st['bullet']),
    ]
    
    t_res_box = Table([[res_desc, img_res1, img_res2]], colWidths=[245, 165, 85])
    t_res_box.setStyle(TableStyle([
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('ALIGN', (1,0), (-1,-1), 'CENTER'),
        ('BACKGROUND', (0,0), (-1,-1), COLOR_BG_CARD),
        ('ROUNDEDCORNERS', [5, 5, 5, 5]),
        ('TOPPADDING', (0,0), (-1,-1), 8),
        ('BOTTOMPADDING', (0,0), (-1,-1), 8),
        ('LEFTPADDING', (0,0), (-1,-1), 8),
        ('RIGHTPADDING', (0,0), (-1,-1), 8),
    ]))
    story.append(t_res_box)

    story.append(PageBreak())

    # ====================================================
    # TRANG 9: HƯỚNG DẪN HỌC LỆNH IR AC / FAN TRÊN WEB CONFIG
    # ====================================================
    story.append(Paragraph("6. Học Lệnh IR Điều Hòa & Quạt Trên Web Config", st['h1']))
    story.append(Paragraph(
        "Trình quản trị Web Config tích hợp sẵn thuật toán phân tích xung thông minh, cho phép sao chép mã điều khiển "
        "từ bất kỳ remote vật lý nào (kể cả quạt nội địa hoặc điều hòa đa năng) vào hệ sinh thái Apple HomeKit.",
        st['body']
    ))
    story.append(Spacer(1, 4))

    # Hộp truy cập & Chuẩn bị
    access_info = [
        [
            Paragraph("<b>Cách truy cập Web Config</b>", st['h2']),
            Paragraph("<b>Chuẩn bị trước khi học lệnh</b>", st['h2'])
        ],
        [
            Paragraph(
                "• Mở trình duyệt gõ: <b>http://nexusir-xxxx.local</b> (hoặc IP hub).<br/>"
                "• Hoặc gạt nút ảo <b>Web Config</b> trên app Apple Home.<br/>"
                "• Khi ở chế độ SoftAP: Truy cập <b>http://192.168.4.1</b>.",
                st['body']
            ),
            Paragraph(
                "• Đặt remote hướng thẳng vào mắt thu IR ở khoảng cách <b>5–10 cm</b>.<br/>"
                "• Tránh ánh sáng mặt trời chiếu trực tiếp làm nhiễu xung.<br/>"
                "• Đèn LED hub sẽ chuyển sang màu <b>Tím nhịp thở</b> khi lắng nghe.",
                st['body']
            )
        ]
    ]
    t_access = Table(access_info, colWidths=[245, 250])
    t_access.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), COLOR_BG_CARD),
        ('GRID', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('TOPPADDING', (0,0), (-1,-1), 5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 5),
        ('LEFTPADDING', (0,0), (-1,-1), 8),
        ('RIGHTPADDING', (0,0), (-1,-1), 8),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
    ]))
    story.append(t_access)
    story.append(Spacer(1, 8))

    # Khối học lệnh Quạt (FAN)
    story.append(Paragraph("A. Quy trình học lệnh Quạt (FAN Wizard - Lưu NVS)", st['h2']))
    story.append(Paragraph(
        "Quạt sử dụng cơ chế lưu mã lệnh đơn lẻ tương ứng với từng trạng thái chức năng:",
        st['body']
    ))
    fan_steps = [
        Paragraph("1. Trên Web Config, bấm <b>(+) Thêm thiết bị</b> -> Chọn <b>Quạt (FAN)</b> -> Nhập tên (vd: <i>Fan_LivingRoom</i>).", st['bullet']),
        Paragraph("2. Hệ thống lần lượt nhắc học từng phím: <b>Nguồn</b> -> <b>Tốc độ 1 (Chậm)</b> -> <b>Tốc độ 2 (Vừa)</b> -> <b>Tốc độ 3 (Mạnh)</b> -> <b>Đảo gió (Swing)</b>.", st['bullet']),
        Paragraph("3. Nếu remote không có phím nào, bấm nút <b>Bỏ qua (Skip)</b>. Sau phím cuối cùng, dữ liệu tự ghi vào NVS và phụ kiện Quạt xuất hiện ngay trên Home app.", st['bullet']),
    ]
    for fs in fan_steps:
        story.append(fs)
    story.append(Spacer(1, 6))

    # Khối học lệnh Điều hòa (AC Matrix)
    story.append(Paragraph("B. Quy trình học ma trận Điều hòa (AC Matrix Wizard - Lưu SPIFFS)", st['h2']))
    story.append(Paragraph(
        "<b>Bản chất kỹ thuật:</b> Remote điều hòa gửi toàn bộ gói trạng thái đầy đủ (Nhiệt độ + Chế độ Cool + Tốc độ quạt + Cánh vẫy). "
        "NexusIR giải quyết bằng cách lập <b>Ma trận xung nhị phân (Binary Matrix)</b> từ 16°C đến 30°C lưu trữ tại phân vùng tốc độ cao `/spiffs/ir_matrix/`.",
        st['body']
    ))
    ac_steps = [
        Paragraph("<b>1. Khởi tạo:</b> Bấm <b>(+) Thêm thiết bị</b> -> Chọn <b>Điều hòa (AC)</b> -> Đặt tên thiết bị -> Bấm <b>Tiếp theo</b>.", st['bullet']),
        Paragraph("<b>2. Bước 0 (Tắt nguồn):</b> Hướng remote vào hub, bấm nút <b>TẮT</b> (Power Off) trên remote gốc.", st['bullet']),
        Paragraph("<b>3. Bước 1 (Mốc chuẩn 16°C Cool - Rất quan trọng):</b> Dùng tay <b>che kín mắt phát của remote</b>, bật remote lên và chỉnh màn hình về <b>COOL, 16°C</b>. Sau đó bỏ tay che, hướng vào hub bấm phím Gửi/Nguồn để nạp mã chuẩn mốc 16°C.", st['bullet']),
        Paragraph("<b>4. Bước 2 đến 15 (Quét dải 17°C -> 30°C):</b> Giữ nguyên remote hướng vào hub. Mỗi khi màn hình web nhắc nhiệt độ kế tiếp, chỉ cần bấm nút <b>Tăng nhiệt độ (+)</b> trên remote 1 nấc. Thanh tiến trình sẽ nhảy từ <b>0/15</b> đến <b>15/15</b>.", st['bullet']),
        Paragraph("<b>5. Hoàn tất:</b> Tệp nhị phân ma trận được ghi tự động. Giờ đây bạn có thể kéo thanh nhiệt độ trên Apple Home hoặc ra lệnh Siri (vd: <i>\"Siri, đặt điều hòa 24 độ\"</i>), hub sẽ phát đúng xung từ ma trận.", st['bullet']),
    ]
    for ac in ac_steps:
        story.append(ac)
    story.append(Spacer(1, 4))
    story.append(Paragraph("<b>Mẹo chuẩn kỹ sư:</b> Việc che đầu phát remote khi chỉnh mốc ban đầu giúp đảm bảo remote không bắn xung sai lệch giữa các chế độ, tạo ra ma trận điều khiển chuẩn xác 100%.", st['callout']))

    story.append(PageBreak())

    # ====================================================
    # TRANG 10: SỬ DỤNG HẰNG NGÀY & TÙY CHỈNH BẢO QUẢN
    # ====================================================
    story.append(Paragraph("5. Sử Dụng Hằng Ngày", st['h1']))
    
    use_cols = [
        [
            Paragraph("<b>Điều khiển qua Apple Home</b>", st['h2']),
            Paragraph("<b>Ra lệnh giọng nói với Siri</b>", st['h2'])
        ],
        [
            Paragraph("• Chạm nhanh vào biểu tượng để bật/tắt thiết bị tức thì.<br/>• Nhấn giữ biểu tượng Điều hòa để chỉnh nhiệt độ 16°C–30°C và đổi chế độ (Cool/Heat/Auto).<br/>• Kéo thanh trượt độ sáng và chọn sắc thái màu cho dải đèn LED.<br/>• Tự động hiển thị nhiệt độ & độ ẩm phòng trên cùng.", st['body']),
            Paragraph("Hỗ trợ điều khiển rảnh tay tiếng Việt hoặc tiếng Anh trên iPhone, Apple Watch, HomePod:<br/><i>• \"Hey Siri, bật đèn phòng khách.\"</i><br/><i>• \"Hey Siri, đặt điều hòa 24 độ.\"</i><br/><i>• \"Hey Siri, đổi đèn sang màu vàng ấm 50%.\"</i><br/><i>• \"Hey Siri, nhiệt độ phòng ngủ là bao nhiêu?\"</i>", st['body'])
        ]
    ]
    t_use = Table(use_cols, colWidths=[245, 250])
    t_use.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), COLOR_BG_CARD),
        ('GRID', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('TOPPADDING', (0,0), (-1,-1), 5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 5),
        ('LEFTPADDING', (0,0), (-1,-1), 8),
        ('RIGHTPADDING', (0,0), (-1,-1), 8),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
    ]))
    story.append(t_use)
    story.append(Spacer(1, 8))

    story.append(Paragraph("7. Tùy Chỉnh và Bảo Quản", st['h1']))
    
    cfg_data = [
        [Paragraph("<b>Tùy chọn</b>", st['body_bold']), Paragraph("<b>Công dụng</b>", st['body_bold']), Paragraph("<b>Mặc định</b>", st['body_bold']), Paragraph("<b>Khi nào nên đổi</b>", st['body_bold'])],
        [Paragraph("Nút bấm vật lý", st['body']), Paragraph("Đổi hiệu ứng LED / Bật relay", st['body']), Paragraph("Hiệu ứng LED", st['body']), Paragraph("Dùng nút hub như công tắc đèn bàn", st['body'])],
        [Paragraph("Độ sáng tối đa", st['body']), Paragraph("Giới hạn dòng cấp LED WS2812B", st['body']), Paragraph("80% (An toàn)", st['body']), Paragraph("Tăng 100% nếu dùng củ sạc trên 3A", st['body'])],
        [Paragraph("Chu kỳ cảm biến", st['body']), Paragraph("Thời gian gửi dữ liệu AHT20", st['body']), Paragraph("30 giây", st['body']), Paragraph("Rút xuống 10s khi cần tự động hóa", st['body'])],
    ]
    t_cfg = Table(cfg_data, colWidths=[95, 140, 95, 165])
    t_cfg.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), COLOR_BG_CARD),
        ('GRID', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('TOPPADDING', (0,0), (-1,-1), 4.5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4.5),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
    ]))
    story.append(t_cfg)
    story.append(Spacer(1, 6))

    story.append(Paragraph("Cập nhật phần mềm (OTA Update)", st['h2']))
    story.append(Paragraph(
        "NexusIR trang bị cơ chế phân vùng kép (Dual-bank OTA: `ota_0` và `ota_1`), tự động hoàn nguyên (rollback) nếu quá trình cập nhật gặp sự cố nguồn điện. "
        "Để cập nhật: Vào Web UI tab <i>Firmware Update</i> -> Tải tệp `.bin` mới -> Bấm <i>Update</i>. Thiết bị sẽ tự nạp và khởi động lại sau 15 giây.",
        st['body']
    ))
    story.append(Spacer(1, 5))
    story.append(Paragraph("<b>Vệ sinh & bảo quản:</b> Dùng khăn vi sợi khô lau bề mặt vòm phát hồng ngoại. Tránh dùng cồn hay hóa chất tẩy rửa mạnh làm mờ bề mặt tán quang. Đặt thiết bị nơi thông thoáng, cách chướng ngại vật ít nhất 30 cm.", st['callout']))

    story.append(PageBreak())

    # ====================================================
    # TRANG 11: XỬ LÝ SỰ CỐ (TROUBLESHOOTING)
    # ====================================================
    story.append(Paragraph("8. Xử Lý Sự Cố (Troubleshooting)", st['h1']))
    story.append(Paragraph("Tra cứu nhanh các tình huống thường gặp trong quá trình lắp đặt và sử dụng:", st['body']))
    story.append(Spacer(1, 4))

    trouble_data = [
        [Paragraph("<b>Hiện tượng</b>", st['body_bold']), Paragraph("<b>Nguyên nhân khả dĩ</b>", st['body_bold']), Paragraph("<b>Cách xử lý từng bước</b>", st['body_bold'])],
        [
            Paragraph("Không lên nguồn, đèn tắt", st['body_bold']),
            Paragraph("Cáp lỏng hoặc củ sạc thiếu dòng", st['body']),
            Paragraph("1. Cắm chặt lại cáp Type-C.<br/>2. Thay củ sạc chuẩn 5V/2A khác.", st['body'])
        ],
        [
            Paragraph("Không thấy Wi-Fi Setup", st['body_bold']),
            Paragraph("Hub đã nhớ Wi-Fi cũ hoặc đang chạy", st['body']),
            Paragraph("<b>Nhấn giữ nút vật lý trên hub trong 5 giây</b> cho đến khi đèn nháy vàng nhanh để reset mạng.", st['body'])
        ],
        [
            Paragraph("Không hiện Captive Portal", st['body_bold']),
            Paragraph("Trình duyệt chặn mở trang tự động", st['body']),
            Paragraph("Đảm bảo đã tích xanh Wi-Fi thiết bị. Mở Safari gõ địa chỉ: <b>192.168.4.1</b>.", st['body'])
        ],
        [
            Paragraph("Báo lỗi kết nối Wi-Fi nhà", st['body_bold']),
            Paragraph("Sai mật khẩu hoặc chọn nhầm sóng 5GHz", st['body']),
            Paragraph("1. Kiểm tra chữ hoa/thường của mật khẩu.<br/>2. Bắt buộc chọn Wi-Fi băng tần <b>2.4 GHz</b>.", st['body'])
        ],
        [
            Paragraph("HomeKit báo 'Không thể thêm'", st['body_bold']),
            Paragraph("Khác mạng Wi-Fi hoặc kẹt bộ nhớ cache", st['body']),
            Paragraph("1. Đảm bảo iPhone cùng mạng 2.4GHz với hub.<br/>2. Đóng hẳn ứng dụng Home mở lại.<br/>3. Nhập mã: <b>111-22-333</b>.", st['body'])
        ],
        [
            Paragraph("Điều hòa không nhận lệnh IR", st['body_bold']),
            Paragraph("Bị vật cản che hoặc chưa đúng mã", st['body']),
            Paragraph("1. Xoay hub hướng về mắt nhận điều hòa.<br/>2. Vào Web UI học lệnh lại từ remote gốc.", st['body'])
        ],
    ]
    t_trouble = Table(trouble_data, colWidths=[120, 130, 245])
    t_trouble.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), COLOR_BG_CARD),
        ('GRID', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('TOPPADDING', (0,0), (-1,-1), 5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 5),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
    ]))
    story.append(t_trouble)
    story.append(Spacer(1, 10))
    story.append(Paragraph("<b>Vẫn chưa giải quyết được?</b> Ghi lại trạng thái đèn LED báo hiệu, mở Web UI chụp màn hình tab <i>Hệ thống (System Info)</i> và gửi về kênh hỗ trợ kỹ thuật ở trang sau.", st['callout']))

    story.append(PageBreak())

    # ====================================================
    # TRANG 12: CÂU HỎI THƯỜNG GẶP, THÔNG SỐ & HỖ TRỢ
    # ====================================================
    story.append(Paragraph("9. Câu Hỏi Thường Gặp (FAQ)", st['h1']))
    
    faqs = [
        ("Mất kết nối Internet thì hub có điều khiển được không?",
         "<b>Có.</b> Toàn bộ giao thức Apple HomeKit và Web UI xử lý 100% trong mạng cục bộ (LAN). Bạn vẫn điều khiển bình thường trong nhà ngay cả khi đứt cáp quang Internet."),
        ("Một thiết bị NexusIR điều khiển được bao nhiêu thiết bị?",
         "Một hub điều khiển được tất cả thiết bị hồng ngoại trong cùng phòng (tối đa 1 Điều hòa, 1 Quạt, 1 TV) cùng 5 dải đèn LED WS2812B độc lập."),
        ("Làm sao để điều khiển khi ra ngoài nhà?",
         "Bạn cần trang bị một thiết bị làm <b>Trung tâm nhà (Home Hub)</b> của Apple như Apple TV 4K, HomePod hoặc iPad đặt cố định tại nhà. Ứng dụng Home sẽ tự động kết nối từ xa qua iCloud mã hóa đầu cuối."),
        ("Điện thoại Android có dùng được không?",
         "<b>Có.</b> NexusIR hỗ trợ đồng thời nền tảng <b>ESP RainMaker</b> trên Android. Bạn có thể tải app trên Google Play và liên kết với Google Home hoặc Alexa."),
    ]
    for q, a in faqs:
        story.append(Paragraph(f"<b>• {q}</b>", st['h2']))
        story.append(Paragraph(a, st['body']))
        story.append(Spacer(1, 2))

    story.append(Spacer(1, 8))
    story.append(Paragraph("10. Thông Số Kỹ Thuật và Hỗ Trợ", st['h1']))
    
    spec_data = [
        [Paragraph("<b>Hạng mục</b>", st['body_bold']), Paragraph("<b>Thông số chi tiết</b>", st['body_bold'])],
        [Paragraph("Tên sản phẩm", st['body_bold']), Paragraph("NexusIR Smart Hub (Bộ Điều Khiển IoT Đa Năng)", st['body'])],
        [Paragraph("Mã thiết bị", st['body_bold']), Paragraph("NX-IR-01 (ESP32-C3 / ESP32-S3)", st['body'])],
        [Paragraph("Vi điều khiển", st['body_bold']), Paragraph("32-bit RISC-V 160MHz hoặc Dual-Core Xtensa LX7 240MHz", st['body'])],
        [Paragraph("Nguồn cấp", st['body_bold']), Paragraph("5V DC ± 5%, cổng cắm USB Type-C (Khuyến nghị củ sạc ≥ 2A)", st['body'])],
        [Paragraph("Kết nối không dây", st['body_bold']), Paragraph("Wi-Fi 802.11 b/g/n (2.4 GHz) · Bluetooth LE 5.0 · ESP-NOW P2P Mesh", st['body'])],
        [Paragraph("Giao thức nhà thông minh", st['body_bold']), Paragraph("Apple HomeKit (HAP) · ESP RainMaker · Web REST API · mDNS", st['body'])],
        [Paragraph("Hồng ngoại phát (IR TX)", st['body_bold']), Paragraph("Mảng đa hướng 360°, bước sóng 940nm, tầm xa hiệu dụng 8–10 mét", st['body'])],
        [Paragraph("Hồng ngoại thu (IR RX)", st['body_bold']), Paragraph("Mắt thu demodulator 38 kHz, góc nhận 120° phục vụ học lệnh", st['body'])],
        [Paragraph("Ngõ ra LED & Relay", st['body_bold']), Paragraph("Tối đa 5 dải LED WS2812B RGB + 2 tiếp điểm Relay tải cao", st['body'])],
        [Paragraph("Cảm biến môi trường", st['body_bold']), Paragraph("AHT20: Đo nhiệt độ (±0.3°C) và độ ẩm (±2% RH) theo thời gian thực", st['body'])],
        [Paragraph("Kích thước & Vỏ hộp", st['body_bold']), Paragraph("82 mm × 82 mm × 45 mm (Vỏ in 3D chống cháy PETG/PLA+)", st['body'])],
    ]
    t_spec = Table(spec_data, colWidths=[150, 345])
    t_spec.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), COLOR_BG_CARD),
        ('GRID', (0,0), (-1,-1), 0.5, COLOR_BORDER),
        ('TOPPADDING', (0,0), (-1,-1), 3.5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3.5),
        ('LEFTPADDING', (0,0), (-1,-1), 7),
        ('RIGHTPADDING', (0,0), (-1,-1), 7),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
    ]))
    story.append(t_spec)
    story.append(Spacer(1, 8))

    story.append(Paragraph("Kênh Hỗ Trợ Kỹ Thuật", st['h2']))
    support_data = [
        [Paragraph("<b>Mã nguồn & Tài liệu:</b>", st['body_bold']), Paragraph("https://github.com/Hdchipeo/NexusIR", st['body'])],
        [Paragraph("<b>Email kỹ sư phụ trách:</b>", st['body_bold']), Paragraph("tranvanchot73@gmail.com", st['body'])],
        [Paragraph("<b>Cập nhật phần mềm:</b>", st['body_bold']), Paragraph("Trực tiếp qua tab Firmware Update trên Web UI nội bộ", st['body'])],
    ]
    t_sup = Table(support_data, colWidths=[150, 345])
    t_sup.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), COLOR_BG_CARD),
        ('ROUNDEDCORNERS', [4, 4, 4, 4]),
        ('TOPPADDING', (0,0), (-1,-1), 4),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ('LEFTPADDING', (0,0), (-1,-1), 7),
        ('RIGHTPADDING', (0,0), (-1,-1), 7),
    ]))
    story.append(t_sup)

    # Build Document
    doc.build(story, canvasmaker=AppleNumberedCanvas)
    print("NexusIR Guide PDF compiled successfully at:", pdf_path)

if __name__ == "__main__":
    create_nexusir_pdf()
