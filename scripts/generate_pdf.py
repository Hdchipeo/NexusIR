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
            self.drawString(54, 800, "Hướng Dẫn Cài Đặt Đèn HomeKit - NexusIR")
            self.setStrokeColor(colors.HexColor("#E2E8F0"))
            self.setLineWidth(0.5)
            self.line(54, 792, 541, 792) # A4 width is 595. Margins are 54, so right margin is 595-54 = 541.
            
        # Draw footer
        page_text = f"Trang {self._pageNumber} / {total_pages}"
        self.drawRightString(541, 40, page_text)
        self.drawString(54, 40, "© 2026 NexusIR. Tất cả quyền được bảo lưu.")
        self.setStrokeColor(colors.HexColor("#E2E8F0"))
        self.setLineWidth(0.5)
        self.line(54, 52, 541, 52)
        
        self.restoreState()

def create_guide_pdf():
    pdf_path = "docs/Huong_Dan_Set_Up_HomeKit_Lamp.pdf"
    os.makedirs(os.path.dirname(pdf_path), exist_ok=True)
    
    # Page setup
    # Margins: 54 points (0.75 in) Left/Right, 54 points Top/Bottom
    doc = SimpleDocTemplate(
        pdf_path,
        pagesize=A4,
        leftMargin=54,
        rightMargin=54,
        topMargin=54,
        bottomMargin=60
    )
    
    styles = getSampleStyleSheet()
    
    # Custom styles supporting Vietnamese (Arial)
    title_style = ParagraphStyle(
        'DocTitle',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=24,
        leading=28,
        textColor=colors.HexColor("#1A365D"), # Deep blue
        spaceAfter=15,
        alignment=1 # Centered
    )
    
    subtitle_style = ParagraphStyle(
        'DocSubtitle',
        parent=styles['Normal'],
        fontName='Arial',
        fontSize=12,
        leading=16,
        textColor=colors.HexColor("#4A5568"), # Slate gray
        spaceAfter=30,
        alignment=1 # Centered
    )
    
    h1_style = ParagraphStyle(
        'Heading1',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=16,
        leading=20,
        textColor=colors.HexColor("#2B6CB0"), # Blue
        spaceBefore=15,
        spaceAfter=10,
        keepWithNext=True
    )
    
    h2_style = ParagraphStyle(
        'Heading2',
        parent=styles['Normal'],
        fontName='Arial-Bold',
        fontSize=12,
        leading=16,
        textColor=colors.HexColor("#2D3748"), # Dark slate
        spaceBefore=10,
        spaceAfter=6,
        keepWithNext=True
    )

    body_style = ParagraphStyle(
        'BodyText',
        parent=styles['Normal'],
        fontName='Arial',
        fontSize=10,
        leading=14,
        textColor=colors.HexColor("#2D3748"),
        spaceAfter=10
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
        spaceAfter=5
    )

    callout_style = ParagraphStyle(
        'CalloutText',
        parent=body_style,
        fontName='Arial-Italic',
        textColor=colors.HexColor("#2C5282"),
        backColor=colors.HexColor("#EBF8FF"),
        borderColor=colors.HexColor("#3182CE"),
        borderWidth=1,
        borderPadding=8,
        spaceBefore=10,
        spaceAfter=10,
        borderRadius=4
    )
    
    story = []
    
    # ------------------ COVER HEADER ------------------
    story.append(Spacer(1, 20))
    story.append(Paragraph("HƯỚNG DẪN CÀI ĐẶT ĐÈN THÔNG MINH HOMEKIT", title_style))
    story.append(Paragraph("Tài liệu hướng dẫn kết nối mạng Wi-Fi & đồng bộ Apple HomeKit chi tiết", subtitle_style))
    story.append(Spacer(1, 10))
    
    # ------------------ PRE-REQUISITES ------------------
    story.append(Paragraph("1. Chuẩn Bị Trước Khi Cài Đặt", h1_style))
    story.append(Paragraph("Để quá trình cài đặt diễn ra suôn sẻ, vui lòng đảm bảo các yêu cầu sau:", body_style))
    story.append(Paragraph("• <b>Mạng Wi-Fi 2.4GHz:</b> Thiết bị chỉ hỗ trợ băng tần 2.4GHz. Hãy chắc chắn điện thoại của bạn đang kết nối vào mạng Wi-Fi 2.4GHz trong suốt quá trình cài đặt (không sử dụng mạng 5GHz).", bullet_style))
    story.append(Paragraph("• <b>Thiết bị iOS/iPadOS:</b> Đã cài đặt ứng dụng <b>Home (Nhà)</b> và đăng nhập tài khoản iCloud.", bullet_style))
    story.append(Paragraph("• <b>Kết nối:</b> Bật sẵn Bluetooth và Dịch vụ định vị (Location Services) trên điện thoại.", bullet_style))
    story.append(Paragraph("• <b>Vị trí:</b> Đặt đèn gần bộ định tuyến Wi-Fi (Router) trong quá trình cấu hình để có tín hiệu tốt nhất.", bullet_style))
    story.append(Spacer(1, 10))
    
    # ------------------ PHASE 1: WI-FI PROVISIONING ------------------
    story.append(Paragraph("2. Phần I: Cấu Hình Wi-Fi Cho Thiết Bị (Wi-Fi Provisioning)", h1_style))
    story.append(Paragraph("Thiết bị sử dụng chế độ phát Wi-Fi (Access Point) tạm thời để người dùng kết nối và truyền thông tin cấu hình mạng gia đình.", body_style))
    
    # We will represent the steps in tables. Each step has description on left, and annotated image on right.
    # Images will be scaled. Width=130px, Height = 1024 * (130/473) = 281px.
    # A4 width = 595 - 108 (margins) = 487 pt.
    # Col 1 (text): 320 pt. Col 2 (image): 150 pt.
    
    def get_step_table(step_num, text_p, img_path):
        img = Image(img_path, width=120, height=260)
        
        # Build cell contents
        cell_text = [
            Paragraph(f"<b>Bước {step_num}</b>", h2_style),
            text_p,
            Spacer(1, 10)
        ]
        
        # Table layout
        t = Table([[cell_text, img]], colWidths=[330, 140])
        t.setStyle(TableStyle([
            ('VALIGN', (0,0), (-1,-1), 'TOP'),
            ('BOTTOMPADDING', (0,0), (-1,-1), 15),
            ('TOPPADDING', (0,0), (-1,-1), 10),
            ('ALIGN', (1,0), (1,0), 'CENTER'),
        ]))
        return t

    # Step 1
    p1 = Paragraph(
        "Cấp nguồn cho đèn HomeKit. Trên điện thoại iPhone, mở ứng dụng <b>Cài đặt (Settings) -> Wi-Fi</b>.<br/><br/>"
        "Tìm trong danh sách Wi-Fi lân cận mạng Wi-Fi do thiết bị phát ra có tên định dạng <b>GhostMagic-Setup-xxxx</b> (trong ảnh minh họa là <i>GhostMagic-Setup-598D</i>). Ấn chọn mạng này để bắt đầu kết nối.",
        body_style
    )
    story.append(get_step_table(1, p1, "docs/images/step1_annotated.png"))
    
    # Step 2
    p2 = Paragraph(
        "Đợi điện thoại thiết lập kết nối với mạng Wi-Fi của thiết bị. Khi kết nối thành công, bạn sẽ thấy <b>dấu tích xanh</b> hiển thị cạnh tên mạng Wi-Fi.<br/><br/>"
        "<i>Lưu ý:</i> Dưới tên Wi-Fi sẽ ghi 'Mạng không bảo mật' (Unsecured Network) và điện thoại có thể cảnh báo không có kết nối Internet. Điều này là hoàn toàn bình thường vì đây chỉ là mạng cục bộ tạm thời để cài đặt.",
        body_style
    )
    story.append(get_step_table(2, p2, "docs/images/step2_annotated.png"))
    
    story.append(PageBreak()) # Clean page break for steps 3, 4, 5
    
    # Step 3
    p3 = Paragraph(
        "Sau khi kết nối thành công, màn hình đăng nhập mạng <b>Captive Portal (Wi-Fi xác thực)</b> sẽ tự động hiển thị trên điện thoại.<br/><br/>"
        "Nếu màn hình này không tự xuất hiện sau 10 giây, bạn hãy mở trình duyệt web (Safari/Chrome) và truy cập địa chỉ IP: <b>192.168.4.1</b> để vào trang cấu hình.<br/><br/>"
        "Tại đây, bạn sẽ thấy danh sách các mạng Wi-Fi quét được xung quanh. Hãy <b>chọn Wi-Fi nhà bạn</b> (phải là mạng 2.4GHz, ví dụ ở đây là <i>DEVICE_2.4G</i>).",
        body_style
    )
    story.append(get_step_table(3, p3, "docs/images/step3_annotated.png"))
    
    # Step 4
    p4 = Paragraph(
        "Một hộp thoại yêu cầu nhập mật khẩu sẽ xuất hiện.<br/><br/>"
        "Hãy <b>nhập chính xác mật khẩu Wi-Fi</b> nhà bạn vào ô nhập (<b>4a</b>), sau đó nhấn nút <b>Join</b> (<b>4b</b>) ở góc phải dưới của hộp thoại để thiết bị ghi nhận thông tin cấu hình.",
        body_style
    )
    story.append(get_step_table(4, p4, "docs/images/step4_annotated.png"))
    
    story.append(PageBreak()) # Clean page break for step 5 and Phase 2

    # Step 5
    p5 = Paragraph(
        "Sau khi nhấn Join, thiết bị sẽ lưu cấu hình Wi-Fi vào bộ nhớ Flash và tự động khởi động lại (Reboot).<br/><br/>"
        "Màn hình sẽ hiển thị thông báo toast màu đen ở dưới cùng: <b>Success! Rebooting...</b> báo hiệu lưu cấu hình thành công.<br/><br/>"
        "Đèn lúc này sẽ tắt mạng Access Point tạm thời và tự kết nối vào mạng Wi-Fi gia đình bạn. Lúc này điện thoại của bạn cũng sẽ tự kết nối lại về mạng Wi-Fi gia đình.",
        body_style
    )
    story.append(get_step_table(5, p5, "docs/images/step5_annotated.png"))
    
    story.append(Spacer(1, 10))

    # ------------------ PHASE 2: HOMEKIT PAIRING ------------------
    story.append(Paragraph("3. Phần II: Thêm Đèn Vào Apple Home (HomeKit Pairing)", h1_style))
    story.append(Paragraph(
        "Sau khi thiết bị đã kết nối thành công vào Wi-Fi nhà bạn (ở Phần I), bước tiếp theo là kết nối nó với ứng dụng Nhà (Home) để điều khiển.",
        body_style
    ))
    
    # HomeKit step descriptions
    story.append(Paragraph("<b>Bước 6:</b> Mở ứng dụng <b>Home (Nhà)</b> trên iPhone/iPad của bạn.", bullet_style))
    story.append(Paragraph("<b>Bước 7:</b> Nhấn vào biểu tượng dấu <b>(+)</b> ở góc trên cùng bên phải và chọn <b>Thêm Phụ Kiện (Add Accessory)</b>.", bullet_style))
    story.append(Paragraph("<b>Bước 8:</b> Tiến hành <b>Quét mã QR Code</b> của HomeKit (được dán trên thân đèn hoặc vỏ hộp). Nếu không có mã QR, bạn hãy chọn <i>'Tôi không có mã hoặc không thể quét'</i> và <b>nhập mã cài đặt 8 số</b> (HomeKit Setup Code) đi kèm thiết bị.", bullet_style))
    story.append(Paragraph("<b>Bước 9:</b> Đợi ứng dụng Home tìm kiếm và kết nối với đèn. Chọn phòng đặt đèn (ví dụ: Phòng khách, Phòng ngủ), đặt tên gợi nhớ cho đèn và ấn <b>Hoàn tất (Done)</b>.", bullet_style))
    
    story.append(Paragraph("💡 <b>Gợi ý điều khiển bằng giọng nói:</b> Sau khi thêm thành công, bạn có thể sử dụng Siri để ra lệnh điều khiển bằng tiếng Anh/tiếng Việt (nếu Siri hỗ trợ), ví dụ: <i>'Hey Siri, turn on Living Room Light'</i>.", callout_style))
    
    # ------------------ TROUBLESHOOTING ------------------
    story.append(Spacer(1, 10))
    story.append(Paragraph("4. Hướng Dẫn Khắc Phục Sự Cố (Troubleshooting)", h1_style))
    
    trouble_data = [
        [
            Paragraph("<b>Vấn đề thường gặp</b>", h2_style),
            Paragraph("<b>Giải pháp xử lý</b>", h2_style)
        ],
        [
            Paragraph("Không thấy Wi-Fi <i>GhostMagic-Setup-xxxx</i> phát ra", body_bold_style),
            Paragraph("1. Kiểm tra nguồn cấp cho đèn.<br/>2. Tiến hành Reset cứng thiết bị: Bật/tắt công tắc nguồn liên tục 5 lần (mỗi lần cách nhau 1-2 giây) cho đến khi đèn nhấp nháy báo hiệu đã chuyển sang chế độ Setup Mode.", body_style)
        ],
        [
            Paragraph("Không tự động hiện trang đăng nhập (Captive Portal)", body_bold_style),
            Paragraph("Đảm bảo điện thoại đã hiển thị dấu tích xanh cạnh mạng <i>GhostMagic-Setup-xxxx</i>. Sau đó mở trình duyệt web độc lập (Safari hoặc Chrome) và gõ trực tiếp địa chỉ: <b>192.168.4.1</b> vào thanh địa chỉ rồi nhấn Go/Enter.", body_style)
        ],
        [
            Paragraph("Đèn không kết nối được vào mạng Wi-Fi nhà bạn", body_bold_style),
            Paragraph("1. Đảm bảo bạn đã nhập chính xác mật khẩu Wi-Fi (kiểm tra kỹ chữ hoa, chữ thường, ký tự đặc biệt).<br/>2. Kiểm tra bộ phát Wi-Fi nhà bạn: Bắt buộc phải bật băng tần <b>2.4GHz</b> (nhiều bộ phát chạy gộp băng tần có thể gây khó kết nối, hãy tách riêng SSID 2.4GHz nếu cần).", body_style)
        ],
        [
            Paragraph("Ứng dụng Home báo lỗi 'Không thể thêm phụ kiện'", body_bold_style),
            Paragraph("1. Đảm bảo điện thoại iPhone đang kết nối cùng một mạng Wi-Fi nhà bạn đã cấu hình cho đèn ở Bước 4.<br/>2. Reset lại đèn và tiến hành cấu hình lại Wi-Fi.<br/>3. Khởi động lại ứng dụng Home hoặc khởi động lại iPhone và thử lại.", body_style)
        ]
    ]
    
    t_trouble = Table(trouble_data, colWidths=[150, 320])
    t_trouble.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#E2E8F0")),
        ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
        ('BOTTOMPADDING', (0,0), (-1,-1), 8),
        ('TOPPADDING', (0,0), (-1,-1), 8),
        ('LEFTPADDING', (0,0), (-1,-1), 8),
        ('RIGHTPADDING', (0,0), (-1,-1), 8),
    ]))
    
    story.append(t_trouble)
    
    # Build Document
    doc.build(story, canvasmaker=NumberedCanvas)
    print("PDF guide compiled successfully at:", pdf_path)

if __name__ == "__main__":
    create_guide_pdf()
