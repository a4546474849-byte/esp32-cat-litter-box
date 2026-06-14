from PIL import Image

src = r"C:\Users\1\Pictures\Screenshots\кот.png"
W, H = 240, 175

img = Image.open(src).convert("RGB").resize((W, H), Image.LANCZOS)
pixels = list(img.getdata())

values = []
for r, g, b in pixels:
    if r > 230 and g > 230 and b > 230:
        r, g, b = 0, 0, 0
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    swapped = ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)
    values.append(swapped)

lines = []
lines.append("#pragma once")
lines.append(f"#define CAT_IMG_W {W}")
lines.append(f"#define CAT_IMG_H {H}")
lines.append(f"const uint16_t cat_img[{W*H}] = {{")  # no PROGMEM on ESP32
row = []
for i, v in enumerate(values):
    row.append(f"0x{v:04X}")
    if len(row) == 16:
        lines.append("  " + ", ".join(row) + ",")
        row = []
if row:
    lines.append("  " + ", ".join(row))
lines.append("};")

out = r"C:\ESP32\src\cat_img.h"
with open(out, "w") as f:
    f.write("\n".join(lines))
print(f"Done: {W}x{H} -> {out}")
