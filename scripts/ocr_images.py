import os
import pytesseract
from PIL import Image

# Set tesseract cmd path explicitly if needed, but it is in path.
# pytesseract.pytesseract.tesseract_cmd = '/opt/homebrew/bin/tesseract'

base_path = 'picture'
files = sorted([f for f in os.listdir(base_path) if f.lower().endswith(('.png', '.jpg'))])

print(f"Found {len(files)} files to OCR:")
for f in files:
    full_path = os.path.join(base_path, f)
    try:
        img = Image.open(full_path)
        # To speed up OCR, we can resize the image to a lower resolution or do full OCR.
        # Since it is fast, let's run OCR.
        text = pytesseract.image_to_string(img, lang='eng')
        # Clean up text
        lines = [line.strip() for line in text.split('\n') if line.strip()]
        snippet = " | ".join(lines[:6]) # Join first few lines
        print(f"{f}: {snippet[:200]}")
    except Exception as e:
        print(f"Error OCR {f}: {e}")
