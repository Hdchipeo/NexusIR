import os
from PIL import Image, ImageDraw, ImageFont

img = Image.open("docs/images/step1_raw.png")
draw = ImageDraw.Draw(img)

# Draw horizontal lines and coordinates
for y in range(0, img.height, 50):
    draw.line([(0, y), (img.width, y)], fill="red", width=1)
    draw.text((10, y + 2), str(y), fill="red")

for x in range(0, img.width, 50):
    draw.line([(x, 0), (x, img.height)], fill="blue", width=1)
    draw.text((x + 2, 10), str(x), fill="blue")

img.save("docs/images/step1_grid.png")
print("Grid image saved.")
