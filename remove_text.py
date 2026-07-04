"""
Убираем текстовые подписи с кадров
Текст в правом верхнем углу (%, Frame N) и правом нижнем (номера)
"""
from PIL import Image, ImageDraw
import os

DIR = r"C:\ESP32\ufo_anim\ufo_new_frames"

for i in range(8):
    path = os.path.join(DIR, f"frame_{i:02d}.png")
    img = Image.open(path).convert("RGB")
    draw = ImageDraw.Draw(img)
    w, h = img.size

    # Правый верхний угол — закрашиваем ~90x50px
    draw.rectangle([w-100, 0, w, 55], fill=(0, 0, 0))
    # Правый нижний угол — закрашиваем ~90x40px
    draw.rectangle([w-100, h-45, w, h], fill=(0, 0, 0))
    # Левый нижний угол — "Frame 1" и т.д.
    draw.rectangle([0, h-45, 100, h], fill=(0, 0, 0))

    img.save(path)
    print(f"frame_{i:02d}: текст убран")

print("\nГотово!")
