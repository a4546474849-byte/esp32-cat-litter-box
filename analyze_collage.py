"""
Анализ структуры коллажа — ищем белые линии-разделители
"""
from PIL import Image

SRC = r"C:\ESP32\ufo\тарелка 5.jpeg"
img = Image.open(SRC).convert("RGB")
pixels = img.load()
w, h = img.size
print(f"Размер: {w}x{h}")

# Ищем горизонтальные белые линии (столбцы где >80% пикселей белее 200)
print("\n--- Горизонтальные линии (y где много белых пикселей) ---")
for y in range(h):
    white_count = sum(1 for x in range(w) if pixels[x, y][0] > 200 and pixels[x, y][1] > 200 and pixels[x, y][2] > 200)
    if white_count > w * 0.5:
        print(f"  y={y}: {white_count}/{w} белых ({white_count*100//w}%)")

print("\n--- Вертикальные линии (x где много белых пикселей) ---")
for x in range(w):
    white_count = sum(1 for y in range(h) if pixels[x, y][0] > 200 and pixels[x, y][1] > 200 and pixels[x, y][2] > 200)
    if white_count > h * 0.3:
        print(f"  x={x}: {white_count}/{h} белых ({white_count*100//h}%)")
