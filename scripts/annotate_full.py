import os
from PIL import Image, ImageDraw, ImageFont

def annotate_full_images():
    os.makedirs("docs/images", exist_ok=True)
    font_path = "/System/Library/Fonts/Supplemental/Arial.ttf"
    font_bold_path = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
    
    # Load font for high-res images
    try:
        font_large = ImageFont.truetype(font_bold_path, 55)
    except IOError:
        font_large = ImageFont.load_default()

    # Definition of annotations
    # (src_name, dst_name, list_of_boxes)
    # Box format: (x1, y1, x2, y2, label)
    annotations = [
        (
            "picture/IMG_3900.PNG",
            "docs/images/step1_annotated.png",
            [(50, 1560, 1120, 1735, "1")]
        ),
        (
            "picture/IMG_3901.PNG",
            "docs/images/step2_annotated.png",
            [(50, 1120, 1120, 1310, "2")]
        ),
        (
            "picture/IMG_3902.PNG",
            "docs/images/step3_annotated.png",
            [(50, 982, 1120, 1143, "3")]
        ),
        (
            "picture/IMG_3903.PNG",
            "docs/images/step4_annotated.png",
            [
                (207, 853, 962, 988, "4a"),
                (585, 1060, 960, 1225, "4b")
            ]
        ),
        (
            "picture/IMG_3904.PNG",
            "docs/images/step5_annotated.png",
            [(291, 2210, 879, 2410, "5")]
        ),
        (
            "picture/IMG_3907.PNG",
            "docs/images/step6_annotated.png",
            [(450, 180, 1120, 290, "6")]
        ),
        (
            "picture/IMG_3911.PNG",
            "docs/images/step7_annotated.png",
            [(343, 735, 830, 801, "7")]
        ),
        (
            "picture/IMG_3910.PNG",
            "docs/images/step8_annotated.png",
            [(150, 1440, 580, 1530, "8")]
        ),
        (
            "picture/IMG_3913.PNG",
            "docs/images/step9_annotated.png",
            [
                (425, 1717, 745, 1782, "9a"),
                (80, 2280, 1090, 2380, "9b")
            ]
        ),
        (
            "picture/IMG_3914.PNG",
            "docs/images/step10_annotated.png",
            [
                (80, 1720, 1090, 1810, "10a"),
                (80, 2280, 1090, 2380, "10b")
            ]
        ),
        (
            "picture/IMG_3917.PNG",
            "docs/images/step11_annotated.png",
            [(80, 2280, 1090, 2380, "11")]
        )
    ]

    for src_name, dst_name, boxes in annotations:
        print(f"Annotating {src_name} -> {dst_name}")
        img = Image.open(src_name).convert("RGB")
        draw = ImageDraw.Draw(img)
        
        for x1, y1, x2, y2, label in boxes:
            # Draw red rectangle with line width 8
            for i in range(8):
                draw.rectangle([x1 - i, y1 - i, x2 + i, y2 + i], outline=(229, 62, 62)) # #E53E3E red
            
            # Badge dimensions (large scale)
            badge_w, badge_h = 75, 75
            if len(label) > 1:
                badge_w = 105
            
            # Position badge above or next to the box
            bx1, by1 = x1 - 25, y1 - 50
            bx2, by2 = bx1 + badge_w, by1 + badge_h
            
            # Draw badge background
            draw.rounded_rectangle([bx1, by1, bx2, by2], radius=15, fill=(229, 62, 62))
            
            # Draw badge text
            bbox = font_large.getbbox(label)
            text_w = bbox[2] - bbox[0]
            text_h = bbox[3] - bbox[1]
            text_x = bx1 + (badge_w - text_w) // 2
            text_y = by1 + (badge_h - text_h) // 2 - 8 # adjustment for baseline
            draw.text((text_x, text_y), label, fill=(255, 255, 255), font=font_large)

        img.save(dst_name)
        print(f"Saved: {dst_name}")

if __name__ == "__main__":
    annotate_full_images()
