import os
from PIL import Image, ImageDraw, ImageFont

def annotate_images():
    os.makedirs("docs/images", exist_ok=True)
    font_path = "/System/Library/Fonts/Supplemental/Arial.ttf"
    font_bold_path = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
    
    # Load fonts
    try:
        font_large = ImageFont.truetype(font_bold_path, 22)
        font_small = ImageFont.truetype(font_path, 16)
    except IOError:
        font_large = ImageFont.load_default()
        font_small = ImageFont.load_default()

    # Define annotations for each step
    # Format: (raw_image_path, output_image_path, list_of_boxes, step_number)
    # Each box: (x1, y1, x2, y2, label)
    annotations = [
        (
            "docs/images/step1_raw.png",
            "docs/images/step1_annotated.png",
            [(20, 620, 453, 690, "1")],
            "Bước 1: Kết nối Wi-Fi phát ra từ thiết bị (GhostMagic-Setup-xxxx)"
        ),
        (
            "docs/images/step2_raw.png",
            "docs/images/step2_annotated.png",
            [(20, 450, 453, 520, "2")],
            "Bước 2: Chờ thiết bị kết nối thành công và hiện tích xanh"
        ),
        (
            "docs/images/step3_raw.png",
            "docs/images/step3_annotated.png",
            [(20, 397, 453, 461, "3")],
            "Bước 3: Chọn mạng Wi-Fi nhà bạn (băng tần 2.4GHz)"
        ),
        (
            "docs/images/step4_raw.png",
            "docs/images/step4_annotated.png",
            [
                (84, 345, 388, 400, "4a"),
                (236, 430, 413, 480, "4b")
            ],
            "Bước 4: Nhập mật khẩu và nhấn 'Join' để kết nối"
        ),
        (
            "docs/images/step5_raw.png",
            "docs/images/step5_annotated.png",
            [(118, 875, 355, 955, "5")],
            "Bước 5: Thiết bị báo 'Success! Rebooting...' thành công"
        )
    ]

    for raw_path, out_path, boxes, step_title in annotations:
        print(f"Annotating {raw_path} -> {out_path}")
        img = Image.open(raw_path).convert("RGB")
        draw = ImageDraw.Draw(img)
        
        for x1, y1, x2, y2, label in boxes:
            # Draw red rectangle with border width 3
            for i in range(3):
                draw.rectangle([x1 - i, y1 - i, x2 + i, y2 + i], outline=(229, 62, 62)) # #E53E3E red
            
            # Draw small badge
            badge_w, badge_h = 30, 30
            if len(label) > 1:
                badge_w = 42
            
            # Position badge above or next to the box
            bx1, by1 = x1 - 10, y1 - 20
            bx2, by2 = bx1 + badge_w, by1 + badge_h
            
            # Draw badge background (solid red circle or rounded rect)
            draw.rounded_rectangle([bx1, by1, bx2, by2], radius=6, fill=(229, 62, 62))
            
            # Draw badge text (white, centered)
            text_x = bx1 + (badge_w - font_large.getbbox(label)[2]) // 2
            text_y = by1 + (badge_h - font_large.getbbox(label)[3]) // 2 - 1
            draw.text((text_x, text_y), label, fill=(255, 255, 255), font=font_large)

        img.save(out_path)
        print(f"Saved: {out_path}")

if __name__ == "__main__":
    annotate_images()
