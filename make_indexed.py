"""
Конвертация кадров в indexed8 для ESPHome (1 байт/пиксель)
8 кадров 320x240 = 600KB вместо 1.2MB
"""
from PIL import Image
import os

SRC_DIR = r"C:\ESP32\ufo_anim\ufo_new_frames"
OUT_DIR = r"C:\ESP32\ufo_anim\ufo_new_indexed"
os.makedirs(OUT_DIR, exist_ok=True)

for i in range(8):
    src = os.path.join(SRC_DIR, f"frame_{i:02d}.png")
    img = Image.open(src).convert("RGB")
    # Квантизуем до 256 цветов (indexed8)
    indexed = img.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    indexed.save(os.path.join(OUT_DIR, f"frame_{i:02d}.png"))
    print(f"frame_{i:02d}: {indexed.mode}, {indexed.size}")

total = 8 * 320 * 240  # 1 byte/pixel
print(f"\nИтого: {total} байт ({total/1024:.0f} KB)raw + палитры")
