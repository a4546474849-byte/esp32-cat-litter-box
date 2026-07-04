"""
Закрашиваем 20px слева и 20px сверху на ВСЕХ кадрах чёрным
"""
from PIL import Image, ImageDraw
import os

DIR = r"C:\ESP32\ufo_anim\ufo_new_frames"
MARGIN = 20  # пикселей (~5mm на экране)

for i in range(8):
    path = os.path.join(DIR, f"frame_{i:02d}.png")
    img = Image.open(path).convert("RGB")
    draw = ImageDraw.Draw(img)
    w, h = img.size

    # Слева 20px на всю высоту
    draw.rectangle([0, 0, MARGIN - 1, h - 1], fill=(0, 0, 0))
    # Сверху 20px на всю ширину
    draw.rectangle([0, 0, w - 1, MARGIN - 1], fill=(0, 0, 0))

    img.save(path)
    print(f"frame_{i:02d}: закрашено {MARGIN}px слева и сверху")

print("\nГотово!")
