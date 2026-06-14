import os
import math
from PIL import Image, ImageOps

# Пути к файлам
gen_img_path = r"C:\ESP32\кот_кибер.png"
target_png_path = r"C:\ESP32\кот.png"
output_h_path = r"C:\ESP32\src\cat_img.h"

# 1. Сохраняем сгенерированное изображение как кот.png
if os.path.exists(gen_img_path):
    img = Image.open(gen_img_path)
    img.save(target_png_path, "PNG")
    print(f"Image saved to {target_png_path}")
else:
    print(f"Error: Base image not found at {gen_img_path}")
    exit(1)

# 2. Конвертируем изображение для дисплея
W, H = 300, 185
img = Image.open(target_png_path).convert("RGB")

# Масштабируем изображение кота с запасом по высоте (высота 175 вместо 185), чтобы уши гарантированно вошли
H_new = 175
W_new = int(img.width * (H_new / img.height))

# Если ширина превышает 300, масштабируем по ширине
if W_new > W:
    W_new = W
    H_new = int(img.height * (W_new / img.width))

resized_img = img.resize((W_new, H_new), Image.LANCZOS)

# Создаем черный холст
final_img = Image.new("RGB", (W, H), (0, 0, 0))

# Вставляем кота по центру
x_offset = (W - W_new) // 2
y_offset = (H - H_new) // 2
final_img.paste(resized_img, (x_offset, y_offset))

pixels = list(final_img.getdata())

# Настройки формата цвета:
USE_BGR = False      # Выключено
BYTE_SWAP = True     # Порядок байт для SPI отправки

# Координаты центра для круговой виньетки
cx = W / 2.0
cy = H / 2.0

values = []
for idx, (r, g, b) in enumerate(pixels):
    x = idx % W
    y = idx // W
    
    # Считаем стандартное круговое расстояние от центра
    dx = x - cx
    dy = y - cy
    d = math.sqrt(dx*dx + dy*dy)
    
    # Виньетка: радиус полной видимости = 90, полного затухания = 145
    R1 = 90.0
    R2 = 145.0
    
    if d < R1:
        factor = 1.0
    elif d > R2:
        factor = 0.0
    else:
        factor = 1.0 - (d - R1) / (R2 - R1)
        # Сглаживание затухания
        factor = 3 * (factor ** 2) - 2 * (factor ** 3)
        
    r = int(r * factor)
    g = int(g * factor)
    b = int(b * factor)
    
    # Дополнительно делаем фон чисто черным для областей, которые очень темные
    if r < 20 and g < 20 and b < 20:
        r, g, b = 0, 0, 0
        
    if USE_BGR:
        color565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
    else:
        color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        
    if BYTE_SWAP:
        val = ((color565 & 0xFF) << 8) | ((color565 >> 8) & 0xFF)
    else:
        val = color565
        
    values.append(val)

# Записываем cat_img.h
lines = [
    "#pragma once",
    f"#define CAT_IMG_W {W}",
    f"#define CAT_IMG_H {H}",
    f"const uint16_t cat_img[{W*H}] = {{"
]

row = []
for v in values:
    row.append(f"0x{v:04X}")
    if len(row) == 16:
        lines.append("  " + ", ".join(row) + ",")
        row = []
if row:
    lines.append("  " + ", ".join(row))
lines.append("};")

with open(output_h_path, "w") as f:
    f.write("\n".join(lines))
    
print(f"Successfully generated {output_h_path} from {target_png_path} (300x185) with vignette")
