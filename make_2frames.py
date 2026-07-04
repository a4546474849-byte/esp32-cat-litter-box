"""Take first 2 frames from 8, keep 320x240"""
from PIL import Image
import os

SRC = r"C:\ESP32\ufo_anim\frames_fullscreen"
OUT = r"C:\ESP32\ufo_anim\frames_2"
os.makedirs(OUT, exist_ok=True)

for i in range(2):
    src = os.path.join(SRC, f"frame_{i:02d}.png")
    img = Image.open(src)
    img.save(os.path.join(OUT, f"frame_{i:02d}.png"))
    print(f"frame_{i:02d}: {img.size}")

total = 2 * 320 * 240 * 2
print(f"\nTotal: {total} bytes ({total/1024:.0f} KB)")
