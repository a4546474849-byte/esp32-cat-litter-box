"""Convert 8 fullscreen frames to indexed8 for ESPHome (1 byte/pixel)"""
from PIL import Image
import os

SRC = r"C:\ESP32\ufo_anim\frames_fullscreen"
OUT = r"C:\ESP32\ufo_anim\frames_indexed"
os.makedirs(OUT, exist_ok=True)

for i in range(8):
    src = os.path.join(SRC, f"frame_{i:02d}.png")
    img = Image.open(src).convert("RGB").quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    img.save(os.path.join(OUT, f"frame_{i:02d}.png"))
    print(f"frame_{i:02d}: {img.size}, mode={img.mode}")

total = 8 * 320 * 240  # 1 byte per pixel
print(f"\nTotal raw: {total} bytes ({total/1024:.0f} KB)")
print(f"With palette: {total + 8*256*2} bytes ({(total + 8*256*2)/1024:.0f} KB)")
