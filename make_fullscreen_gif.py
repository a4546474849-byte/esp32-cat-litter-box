"""Create GIF from fullscreen 320x240 frames"""
from PIL import Image
import os

FRAMES_DIR = r"C:\ESP32\ufo_anim\frames_fullscreen"
OUT_GIF = r"C:\ESP32\ufo_anim\ufo_fullscreen.gif"

frames = []
for i in range(8):
    path = os.path.join(FRAMES_DIR, f"frame_{i:02d}.png")
    img = Image.open(path).convert("RGB")
    frames.append(img)
    print(f"Loaded frame_{i:02d}: {img.size}")

# Save as GIF, 200ms per frame = 1600ms total cycle
frames[0].save(
    OUT_GIF,
    save_all=True,
    append_images=frames[1:],
    duration=200,
    loop=0,
    optimize=True,
)

size = os.path.getsize(OUT_GIF)
print(f"\nGIF saved: {OUT_GIF}")
print(f"Size: {size} bytes ({size/1024:.1f} KB)")
print(f"Resolution: 320x240, {len(frames)} frames, 200ms each")
