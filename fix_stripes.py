"""
Убираем артефакт-линии с кадров
frame_04: вертикальная линия x=0 (181 пиксель)
frame_07: вертикальная линия x=0 (180 пикселей)
frame_03: минимальные артефакты x=0..3
"""
from PIL import Image, ImageDraw
import os

DIR = r"C:\ESP32\ufo_anim\ufo_new_frames"

for i in range(8):
    path = os.path.join(DIR, f"frame_{i:02d}.png")
    img = Image.open(path).convert("RGB")
    draw = ImageDraw.Draw(img)
    w, h = img.size
    pixels = img.load()

    # Проверяем левые столбцы — если есть ненулевые пиксели, закрашиваем
    needs_fix = False
    for x in range(5):
        col_bright = sum(1 for y in range(h) if max(pixels[x, y]) > 2)
        if col_bright > 3:
            needs_fix = True
            break

    if needs_fix:
        # Закрашиваем первые 5 столбцов чёрным
        draw.rectangle([0, 0, 4, h-1], fill=(0, 0, 0))
        # Также проверяем правые столбцы
        for x in range(w-5, w):
            col_bright = sum(1 for y in range(h) if max(pixels[x, y]) > 2)
            if col_bright > 3:
                draw.rectangle([w-5, 0, w-1, h-1], fill=(0, 0, 0))
                break
        img.save(path)
        print(f"frame_{i:02d}: артефакты убраны")
    else:
        print(f"frame_{i:02d}: чисто")

print("\nГотово!")
