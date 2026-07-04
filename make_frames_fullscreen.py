"""
Кадры анимации взлёта тарелки для CYD (320x240)
Убираем рамки киноплёнки, делаем плавный взлёт
"""
from PIL import Image
import os

SRC = r"C:\ESP32\аниме.png"
OUT_DIR = r"C:\ESP32\ufo_anim\frames_fullscreen"
os.makedirs(OUT_DIR, exist_ok=True)

img = Image.open(SRC).convert("RGB")
src_w, src_h = img.size
print(f"Источник: {src_w}x{src_h}")

# Коллаж 4x2, каждый кадр примерно 250x440
# Рамки плёнки: ~30px сверху и снизу каждого кадра
# Чистая область: примерно 250x380

frame_w = 250
frame_h = 380  # Без рамок плёнки

# Координаты чистых кадров (сдвинуты вниз чтобы убрать верхнюю рамку)
coords = [
    (12, 40),   (266, 40),  (520, 40),  (774, 40),   # Верхний ряд
    (12, 564),  (266, 564), (520, 564), (774, 564)    # Нижний ряд
]

# Порядок для плавного взлёта
takeoff_order = [0, 4, 1, 5, 2, 6, 3, 7]

W, H = 320, 240

for idx, frame_num in enumerate(takeoff_order):
    left, top = coords[frame_num]
    box = (left, top, left + frame_w, top + frame_h)
    frame = img.crop(box)

    # Масштабируем по высоте экрана
    scale = H / frame_h
    new_w = int(frame_w * scale)
    new_h = H

    frame_resized = frame.resize((new_w, new_h), Image.LANCZOS)

    # Центрируем на чёрном холсте
    canvas = Image.new("RGB", (W, H), (0, 0, 0))
    x_off = (W - new_w) // 2
    canvas.paste(frame_resized, (x_off, 0))

    canvas.save(os.path.join(OUT_DIR, f"frame_{idx:02d}.png"))
    print(f"Кадр {idx}: {new_w}x{new_h}")

print(f"\nРазмер кадра: {W}x{H}x2 = {W*H*2} байт")
print(f"8 кадров: {8 * W * H * 2 / 1024:.0f} KB")
