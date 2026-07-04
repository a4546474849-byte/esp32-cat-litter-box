"""
Нарезка UFO коллажа 2x4 с правильными координатами
Белые разделители: x=442-453, y=293-303, y=595-606, y=898-909
"""
from PIL import Image
import os

SRC = r"C:\ESP32\ufo\тарелка 5.jpeg"
OUT_DIR = r"C:\ESP32\ufo_anim\ufo_new_frames"
os.makedirs(OUT_DIR, exist_ok=True)

img = Image.open(SRC).convert("RGB")
W, H = 320, 240

# Границы кадров (без белых линий)
# Столбцы: 0..441, 454..895
# Ряды:    0..292, 304..594, 607..897, 910..1199
frames = [
    # Колонка 0 (левая)
    (0, 0, 441, 292),       # кадр 0: верх-лево
    (0, 304, 441, 594),     # кадр 1: второй ряд-лево
    (0, 607, 441, 897),     # кадр 2: третий ряд-лево
    (0, 910, 441, 1199),    # кадр 3: низ-лево
    # Колонка 1 (правая)
    (454, 0, 895, 292),     # кадр 4: верх-право
    (454, 304, 895, 594),   # кадр 5: второй ряд-право
    (454, 607, 895, 897),   # кадр 6: третий ряд-право
    (454, 910, 895, 1199),  # кадр 7: низ-право
]

for i, (l, t, r, b) in enumerate(frames):
    frame = img.crop((l, t, r, b))
    fw, fh = frame.size

    # Вписываем в 320x240
    scale = min(W / fw, H / fh)
    new_w = int(fw * scale)
    new_h = int(fh * scale)
    frame_resized = frame.resize((new_w, new_h), Image.LANCZOS)

    canvas = Image.new("RGB", (W, H), (0, 0, 0))
    x_off = (W - new_w) // 2
    y_off = (H - new_h) // 2
    canvas.paste(frame_resized, (x_off, y_off))

    canvas.save(os.path.join(OUT_DIR, f"frame_{i:02d}.png"))
    print(f"Кадр {i}: ({l},{t})-({r},{b}) = {fw}x{fh} -> {new_w}x{new_h}")

total = 8 * W * H * 2
print(f"\nГотово! 8 кадров {W}x{H} RGB565 = {total/1024:.0f} KB")
