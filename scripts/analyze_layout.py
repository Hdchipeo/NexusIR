from PIL import Image

def analyze(filepath):
    img = Image.open(filepath)
    width, height = img.size
    print(f"Analyzing {filepath} ({width}x{height})")
    
    # Check vertical line in the middle (e.g. x=200) to find colors
    # We want to identify transitions between background (light gray/blueish) and white cards
    col_x = 200
    transitions = []
    prev_color = img.getpixel((col_x, 0))
    
    # Let's check color of each pixel on vertical line
    for y in range(height):
        color = img.getpixel((col_x, y))
        # Since it might be RGB or RGBA
        r, g, b = color[0], color[1], color[2]
        
        # Is it white? (or very close to white)
        is_white = (r > 250 and g > 250 and b > 250)
        
        if y == 0:
            prev_is_white = is_white
            start_y = 0
            continue
            
        if is_white != prev_is_white:
            print(f"  y={start_y} to {y-1}: {'WHITE' if prev_is_white else 'BG'} (sample color: {prev_color[:3]})")
            start_y = y
            prev_is_white = is_white
            prev_color = color
            
    print(f"  y={start_y} to {height-1}: {'WHITE' if prev_is_white else 'BG'} (sample color: {prev_color[:3]})")

analyze("docs/images/step1_raw.png")
analyze("docs/images/step2_raw.png")
analyze("docs/images/step3_raw.png")
