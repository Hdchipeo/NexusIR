import os
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

# Define fonts
FONT_REGULAR = "/System/Library/Fonts/Supplemental/Arial.ttf"
FONT_BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
FONT_ITALIC = "/System/Library/Fonts/Supplemental/Arial Italic.ttf"

pdfmetrics.registerFont(TTFont('Arial', FONT_REGULAR))
pdfmetrics.registerFont(TTFont('Arial-Bold', FONT_BOLD))
pdfmetrics.registerFont(TTFont('Arial-Italic', FONT_ITALIC))

class NumberedCanvas(canvas.Canvas):
    """
    Two-pass canvas to dynamically compute and print the total page count.
    Useful for 'Page X of Y' page numbers.
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
        self.setFont("Arial", 8)
        self.setFillColor(colors.HexColor("#718096"))
        
        # Draw header (on pages after the first page)
        if self._pageNumber > 1:
            self.drawString(45, 800, "Hướng Dẫn Thiết Lập Đèn HomeKit - NexusIR")
            self.setStrokeColor(colors.HexColor("#E2E8F0"))
            self.setLineWidth(0.5)
            self.line(45, 792, 550, 792) # A4 width is 595. Margins are 45, so right margin is 595-45 = 550.
            
        # Draw footer
        page_text = f"Trang {self._pageNumber} / {total_pages}"
        self.drawRightString(550, 40, page_text)
        self.drawString(45, 40, "© 2026 NexusIR. Hướng dẫn thiết lập rút gọn.")
        self.setStrokeColor(colors.HexColor("#E2E8F0"))
        self.setLineWidth(0.5)
        self.line(45, 52, 550, 52)
        
        self.restoreState()

def create_guide_pdf():
    pdf_path = "docs/Huong_Dan_Set_Up_HomeKit_Lamp.pdf"
    os.makedirs(os.path.dirname(pdf_path), exist_ok=True)
    
    # Page setup (margins: 45 pt)
    doc = SimpleDocTemplate(
        pdf_path,
        pagesize=A4,
        leftMargin=45,
        rightMargin=45,
        topMargin=50,
        bottomMargin=65
    )
    
    styles = getSampleStyleSheet()
    
    # Custom styles supporting Vietnamese (Arial)
    title_style = ParagraphStyle(
        'DocTitle',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=20,
        leading=24,
        textColor=colors.HexColor("#1A365D"), # Deep blue
        spaceAfter=10,
        alignment=1 # Centered
    )
    
    subtitle_style = ParagraphStyle(
        'DocSubtitle',
        parent=styles['Normal'],
        fontName='Arial-Italic',
        fontSize=11,
        leading=15,
        textColor=colors.HexColor("#4A5568"), # Slate gray
        spaceAfter=20,
        alignment=1 # Centered
    )
    
    h1_style = ParagraphStyle(
        'Heading1',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=13,
        leading=17,
        textColor=colors.HexColor("#2B6CB0"), # Blue
        spaceBefore=12,
        spaceAfter=8,
        keepWithNext=True
    )
    
    h2_style = ParagraphStyle(
        'Heading2',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=11,
        leading=15,
        textColor=colors.HexColor("#2D3748"), # Dark slate
        spaceBefore=6,
        spaceAfter=4,
        keepWithNext=True
    )

    body_style = ParagraphStyle(
        'BodyText',
        parent=styles['Normal'],
        fontName='Arial',
        fontSize=9.5,
        leading=14,
        textColor=colors.HexColor("#2D3748"),
        spaceAfter=8
    )
    
    body_bold_style = ParagraphStyle(
        'BodyTextBold',
        parent=body_style,
        fontName='Arial-Bold'
    )
    
    bullet_style = ParagraphStyle(
        'BulletText',
        parent=body_style,
        leftIndent=15,
        firstLineIndent=-10,
        spaceAfter=4
    )

    callout_style = ParagraphStyle(
        'CalloutText',
        parent=body_style,
        fontName='Arial-Italic',
        textColor=colors.HexColor("#2C5282"),
        backColor=colors.HexColor("#EBF8FF"),
        borderColor=colors.HexColor("#3182CE"),
        borderWidth=0.8,
        borderPadding=6,
        spaceBefore=8,
        spaceAfter=8,
        borderRadius=4
    )
    
    story = []
    
    # ------------------ COVER HEADER ------------------
    story.append(Spacer(1, 10))
    story.append(Paragraph("HƯỚNG DẪN CÀI ĐẶT ĐÈN HOMEKIT", title_style))
    story.append(Paragraph("Tài liệu hướng dẫn kết nối mạng Wi-Fi và đồng bộ Apple HomeKit rút gọn", subtitle_style))
    story.append(Spacer(1, 5))
    
    # ------------------ PRE-REQUISITES ------------------
    story.append(Paragraph("1. Chuẩn Bị Trước Khi Cài Đặt", h1_style))
    story.append(Paragraph("• <b>Mạng Wi-Fi 2.4GHz:</b> Bắt buộc kết nối điện thoại vào Wi-Fi 2.4GHz (không dùng mạng 5GHz).", bullet_style))
    story.append(Paragraph("• <b>Điện thoại iPhone:</b> Đã cài sẵn ứng dụng <b>Home (Nhà)</b> và bật sẵn Bluetooth, định vị.", bullet_style))
    story.append(Spacer(1, 8))
    
    # Helper to generate step table with description and screenshot
    def get_step_table(step_num, text_p, img_path):
        img = Image(img_path, width=120, height=260)
        cell_text = [
            Paragraph(f"<b>Bước {step_num}</b>", h2_style),
            text_p,
            Spacer(1, 8)
        ]
        t = Table([[cell_text, img]], colWidths=[360, 140])
        t.setStyle(TableStyle([
            ('VALIGN', (0,0), (-1,-1), 'TOP'),
            ('BOTTOMPADDING', (0,0), (-1,-1), 10),
            ('TOPPADDING', (0,0), (-1,-1), 5),
            ('ALIGN', (1,0), (1,0), 'CENTER'),
        ]))
        return t

    # ------------------ PHASE 1: WI-FI PROVISIONING ------------------
    story.append(Paragraph("2. Phần I: Cấu Hình Wi-Fi Cho Thiết Bị (Wi-Fi Provisioning)", h1_style))
    
    # Step 1
    p1 = Paragraph(
        "Mở cài đặt Wi-Fi trên iPhone.<br/><br/>"
        "Tìm và chọn mạng Wi-Fi do thiết bị phát ra có tên định dạng: <b>GhostMagic-Setup-xxxx</b> (trong ảnh là <i>GhostMagic-Setup-598D</i>).",
        body_style
    )
    story.append(get_step_table(1, p1, "docs/images/step1_annotated.png"))
    story.append(PageBreak()) # Page break for Step 2 & 3

    # Step 2
    p2 = Paragraph(
        "Chờ kết nối thành công với Wi-Fi của thiết bị (hiển thị <b>dấu tích xanh</b> ở đầu).<br/><br/>"
        "<i>Lưu ý:</i> Dưới tên Wi-Fi sẽ ghi cảnh báo mạng không bảo mật hoặc không có internet, điều này là bình thường.",
        body_style
    )
    story.append(get_step_table(2, p2, "docs/images/step2_annotated.png"))
    story.append(Spacer(1, 10))

    # Step 3
    p3 = Paragraph(
        "Màn hình đăng nhập mạng <b>Captive Portal (Wi-Fi xác thực)</b> sẽ tự động hiển thị.<br/><br/>"
        "Tại danh sách Wi-Fi hiện ra, chọn đúng <b>mạng Wi-Fi nhà bạn</b> (băng tần 2.4GHz, ví dụ là <i>DEVICE_2.4G</i>).",
        body_style
    )
    story.append(get_step_table(3, p3, "docs/images/step3_annotated.png"))
    story.append(PageBreak()) # Page break for Step 4 & 5

    # Step 4
    p4 = Paragraph(
        "Nhập chính xác mật khẩu Wi-Fi nhà bạn vào ô trống (<b>4a</b>).<br/><br/>"
        "Sau đó nhấn nút <b>Join</b> (<b>4b</b>) ở góc dưới bên phải hộp thoại để lưu cấu hình.",
        body_style
    )
    story.append(get_step_table(4, p4, "docs/images/step4_annotated.png"))
    story.append(Spacer(1, 10))

    # Step 5
    p5 = Paragraph(
        "Chờ thiết bị lưu cấu hình và tự khởi động lại.<br/><br/>"
        "Màn hình sẽ hiển thị thông báo toast màu đen ở chân trang: <b>Success! Rebooting...</b> báo hiệu thành công.<br/><br/>"
        "Thiết bị sẽ tự kết nối vào mạng Wi-Fi nhà bạn.",
        body_style
    )
    story.append(get_step_table(5, p5, "docs/images/step5_annotated.png"))
    story.append(PageBreak()) # Page break for Phase 2: Steps 6 & 7

    # ------------------ PHASE 2: HOMEKIT PAIRING ------------------
    story.append(Paragraph("3. Phần II: Đồng Bộ Với Apple HomeKit (HomeKit Pairing)", h1_style))
    
    # Step 6
    p6 = Paragraph(
        "Mở ứng dụng <b>Home (Nhà)</b> trên iPhone.<br/><br/>"
        "Nhấn vào biểu tượng dấu <b>(+)</b> ở góc trên cùng bên phải và chọn <b>Thêm phụ kiện (Add Accessory)</b>.",
        body_style
    )
    story.append(get_step_table(6, p6, "docs/images/step6_annotated.png"))
    story.append(Spacer(1, 10))

    # Step 7
    p7 = Paragraph(
        "Giao diện quét mã hiện lên, chọn <i>'Tôi không có mã hoặc không thể quét'</i> hoặc <i>'Thêm bằng mã...'</i>.<br/><br/>"
        "Tiến hành nhập mã cài đặt 8 số của HomeKit: <b>111-22-333</b> (nhập liền <b>11122333</b>).",
        body_style
    )
    story.append(get_step_table(7, p7, "docs/images/step7_annotated.png"))
    story.append(PageBreak()) # Page break for Steps 8 & 9

    # Step 8
    p8 = Paragraph(
        "Khi xuất hiện hộp thoại cảnh báo phụ kiện chưa được chứng nhận từ Apple.<br/><br/>"
        "Hãy chọn nút <b>Vẫn thêm (Add Anyway)</b> ở phía bên trái để tiếp tục tiến trình đồng bộ.",
        body_style
    )
    story.append(get_step_table(8, p8, "docs/images/step8_annotated.png"))
    story.append(Spacer(1, 10))

    # Step 9
    p9 = Paragraph(
        "Chờ quá trình kết nối cầu nối của thiết bị hoàn tất (màn hình hiển thị <i>Đang thêm vào màn hình chính...</i>).<br/><br/>"
        "Sau đó, <b>chọn Tiếp tục</b> ở phía dưới cùng màn hình.",
        body_style
    )
    story.append(get_step_table(9, p9, "docs/images/step9_annotated.png"))
    story.append(PageBreak()) # Page break for Steps 10 & 11

    # Step 10
    p10 = Paragraph(
        "Đặt tên tùy chỉnh cho Cầu nối (ví dụ: <i>GhostLamp Bridge</i>) và gán phòng tương ứng.<br/><br/>"
        "Sau đó, <b>chọn Tiếp tục</b> (nhấn Xác định/Tiếp tục cho các thiết bị phụ trợ liên kết).",
        body_style
    )
    story.append(get_step_table(10, p10, "docs/images/step10_annotated.png"))
    story.append(Spacer(1, 10))

    # Step 11
    p11 = Paragraph(
        "Hệ thống báo đã thêm thành công phụ kiện đèn thông minh vào Nhà.<br/><br/>"
        "Nhấn chọn <b>Xong (Done)</b> ở góc dưới cùng để hoàn tất quá trình cài đặt và bắt đầu sử dụng.",
        body_style
    )
    story.append(get_step_table(11, p11, "docs/images/step11_annotated.png"))
    story.append(PageBreak()) # Page break for Dashboard & Troubleshooting

    # ------------------ PHASE 3: DASHBOARD & CONTROL ------------------
    story.append(Paragraph("4. Phần III: Giao Diện Điều Khiển", h1_style))
    
    img_res1 = Image("docs/images/result1.jpg", width=190, height=180)
    img_res2 = Image("docs/images/result2.png", width=110, height=238)
    
    res_text = [
        Paragraph("<b>Bảng điều khiển sau khi cài đặt:</b>", h2_style),
        Paragraph("• Thiết bị hiển thị dưới dạng cầu nối <b>GhostLamp Bridge</b> quản lý đèn LED và các cảm biến phụ trợ.", bullet_style),
        Paragraph("• Cho phép điều khiển bật/tắt nhanh, tùy chọn độ sáng, bảng màu sắc RGB đầy đủ của đèn LED.", bullet_style),
        Paragraph("• Nút <b>Web Config</b> dùng để kích hoạt nhanh trang cấu hình web cục bộ của thiết bị.", bullet_style),
        Spacer(1, 10)
    ]
    
    t_res = Table([[res_text, img_res1, img_res2]], colWidths=[200, 190, 110])
    t_res.setStyle(TableStyle([
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
        ('ALIGN', (1,0), (-1,-1), 'CENTER'),
        ('BOTTOMPADDING', (0,0), (-1,-1), 10),
    ]))
    story.append(t_res)
    story.append(Spacer(1, 10))

    # ------------------ TROUBLESHOOTING ------------------
    story.append(Paragraph("5. Hướng Dẫn Khắc Phục Sự Cố (Troubleshooting)", h1_style))
    
    trouble_data = [
        [
            Paragraph("<b>Vấn đề thường gặp</b>", h2_style),
            Paragraph("<b>Giải pháp xử lý</b>", h2_style)
        ],
        [
            Paragraph("Không thấy Wi-Fi <i>GhostMagic-Setup-xxxx</i>", body_bold_style),
            Paragraph("1. Kiểm tra nguồn cấp.<br/>2. Để reset thiết bị về mặc định: <b>Nhấn giữ button vật lý trên thiết bị trong 3 giây</b> cho đến khi đèn nháy để vào chế độ Setup.", body_style)
        ],
        [
            Paragraph("Trang cấu hình Wi-Fi không tự hiển thị", body_bold_style),
            Paragraph("Đảm bảo điện thoại đã kết nối Wi-Fi thiết bị. Mở trình duyệt và truy cập <b>tên miền mDNS</b>: <b>http://ghostmagic.local:8080</b> để mở cấu hình.", body_style)
        ],
        [
            Paragraph("Đèn không kết nối được vào Wi-Fi nhà", body_bold_style),
            Paragraph("Yêu cầu Wi-Fi nhà phải là mạng <b>2.4GHz</b>. Đảm bảo nhập đúng mật khẩu mạng Wi-Fi.", body_style)
        ]
    ]
    
    t_trouble = Table(trouble_data, colWidths=[150, 350])
    t_trouble.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#E2E8F0")),
        ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
        ('BOTTOMPADDING', (0,0), (-1,-1), 6),
        ('TOPPADDING', (0,0), (-1,-1), 6),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t_trouble)
    
    # Build Document
    doc.build(story, canvasmaker=NumberedCanvas)
    print("Full PDF guide compiled successfully at:", pdf_path)

if __name__ == "__main__":
    create_guide_pdf()
