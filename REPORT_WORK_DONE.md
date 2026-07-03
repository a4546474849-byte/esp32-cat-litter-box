# Отчёт о проделанной работе: Smart Cat Litter на ESPHome

**Дата:** 2026-07-02 (обновлено)
**Платформа:** ESP32-2432S028 (CYD)
**Прошивка:** ESPHome 2026.6.2 + LVGL 9.5.0

---

## 1. Что было сделано

### 1.1 Перенос проекта с Arduino на ESPHome
Проект "Smart Cat Litter Box" перенесён с Arduino/PlatformIO на ESPHome:
- Графический интерфейс с кибер-котом на LVGL
- Кнопка "ВЗЛЕТ" (очистка лотка сервоприводом)
- Автомат состояний (радар LD2410C управляет логикой)
- Счётчик посещений с сохранением в EEPROM

### 1.2 Анимация взлёта тарелки (текущая версия)
- 8 кадров покадровой анимации через `animimg` (160x120)
- Тарелка взлетает с огненным следом
- После анимации активируется сервопривод (2 сек)
- Затем возврат к экрану с котом

### 1.3 Интеграция AnimatedGIF (bitbank2/Larry Bank)
Попытка интеграции GIF-плеера через библиотеку AnimatedGIF:
- Библиотека скачана и скомпилирована вместе с проектом
- GIF декодируется из Flash и рисуется напрямую на дисплей
- Исправлены ошибки: PROGMEM, draw_pixel_at, callback signature
- Замена Arduino.h на ESP-IDF эквиваленты (FreeRTOS delay/millis)
- Ленивая инициализация плеера при нажатии кнопки

---

## 2. Решённые проблемы

| # | Проблема | Причина | Решение |
|---|----------|---------|---------|
| 1 | Серый вместо чёрного фона | `invert_colors: true` инвертировал все цвета | `invert_colors: false` |
| 2 | Очки кота жёлтые вместо неоновых | `color_order: BGR` менял R/B каналы | `color_order: RGB` |
| 3 | Экран повёрнут влево | LVGL `rotation: 90` | `rotation: 180` |
| 4 | Кнопка не работает | Тач-координаты не инвертированы | `transform: swap_xy + mirror_x + mirror_y` |
| 5 | Кнопка "VZLET" латиницей | Montserrat без кириллицы | `glyphsets: GF_Cyrillic_Core` + "ВЗЛЕТ" |
| 6 | Полоса прокрутки на экране | LVGL scrollbar по дефолту | `scrollbar_mode: "OFF"` |
| 7 | Зелёный фон по бокам | Дефолтный bg_color страницы | `bg_color: 0x000000` |
| 8 | Артефакты ресайза PNG | ESPHome некорректно ресайзит | Предварительная обрезка через Pillow |
| 9 | Чёрный экран после правок | `auto_clear_enabled: false` блокировал LVGL | Убрать, использовать `reset_pin: GPIO21` |
| 10 | Прошивка не влезает в flash | 8 кадров по 320x240 = 2.3MB > 1.8MB | Уменьшил кадры до 160x120 |
| 11 | AnimatedGIF не компилируется | `#include <Arduino.h>` в ESP-IDF | Замена на FreeRTOS delay/millis |
| 12 | `draw_pixel` vs `draw_pixel_at` | Разные версии ESPHome API | Использовать `draw_pixel_at` |
| 13 | GIF callback signature | Возвращает `int16_t` вместо `void` | Исправлено на `void` |
| 14 | `gif.inl` не распознаётся | ESPHome не принимает `.inl` расширение | Объединение в один `.cpp` файл |
| 15 | UTF-16 BOM в C-массиве | PowerShell кодирует вывод | Прямая запись файла из Python |

---

## 3. Ключевые настройки для CYD

### 3.1 Дисплей ILI9341
```yaml
display:
  - platform: ili9xxx
    model: ILI9341
    cs_pin: GPIO15
    dc_pin: GPIO2
    reset_pin: GPIO21       # Обязательно для сброса
    invert_colors: false     # НЕ true!
    color_order: RGB         # НЕ BGR!
    dimensions: 320x240
```

### 3.2 LVGL
```yaml
lvgl:
  rotation: 180
  buffer_size: 20%
  byte_order: big_endian
  pages:
    - scrollbar_mode: "OFF"  # Убирает полосу прокрутки
```

### 3.3 Тачскрин XPT2046
```yaml
touchscreen:
  calibration: {x_min: 220, x_max: 3756, y_min: 394, y_max: 3749}
  transform:
    swap_xy: true
    mirror_x: true
    mirror_y: true
```

### 3.4 Шрифты с кириллицей
```yaml
font:
  - file: "gfonts://Montserrat"
    glyphsets: GF_Cyrillic_Core
```

### 3.5 Подключение AnimatedGIF
```yaml
esphome:
  includes:
    - AnimatedGIF.h
    - AnimatedGIF_merged.cpp   # AnimatedGIF.cpp + gif.inl объединены
    - cyd_gif.h
    - ufo_gif_data.h           # GIF данные в C-массиве
```

---

## 4. Интеграция AnimatedGIF — архитектура

### Файлы проекта:
```
/home/esphome/
├── cat_litter_gif.yaml         # YAML конфиг (актуальный)
├── AnimatedGIF.h               # Заголовок библиотеки
├── AnimatedGIF_merged.cpp      # AnimatedGIF.cpp + gif.inl (объединены)
├── cyd_gif.h                   # CYDGifPlayer класс + gifDrawCallback
├── ufo_gif_data.h              # GIF данные в C-массиве (PROGMEM)
├── kot_320x240.png             # Картинка кота
└── ufo_anim/
    └── ufo_small.gif           # GIF анимация (26.9 KB, 100x150, 16 цветов)
```

### Цикл работы GIF:
```
Flash (GIF байты) → AnimatedGIF (декодер) → gifDrawCallback() → DisplayBuffer → ILI9341
```

### Ключевые исправления в cyd_gif.h:
1. `#include "AnimatedGIF.h"` (кавычки, не угловые скобки)
2. Callback `void gifDrawCallback(GIFDRAW *pDraw)` — возвращает void
3. `draw_pixel_at()` вместо `draw_pixel()`
4. Нет `ucDirect` — используем `pPalette` напрямую
5. `gif.begin()` без аргументов (дефолт RGB565_LE)
6. `gif.open()` с RAW данными из Flash

### Конвертация GIF в C-массив:
```bash
python3 gif_to_c.py input.gif output.h
```

---

## 5. Процесс прошивки

### Быстрый цикл (изменения в YAML):
```bash
cd /home/esphome
esphome compile cat_litter_v2.yaml    # ~35 сек (инкрементальная)
esphome upload cat_litter_v2.yaml --device 192.168.1.16  # ~7 сек OTA
```

### Полная пересборка:
```bash
rm -rf .esphome/build/smart-cat-litter
esphome compile cat_litter_v2.yaml    # ~150 сек
```

---

## 6. Расположение файлов

### WSL (рабочая папка):
- `/home/esphome/cat_litter_gif.yaml` — конфиг с GIF
- `/home/esphome/cat_litter_v2.yaml` — конфиг с animimg
- `/home/esphome/kot_320x240.png` — картинка кота
- `/home/esphome/ufo_frames/` — кадры анимации
- `/home/esphome/ufo_anim/ufo_small.gif` — GIF тарелки

### Windows:
- `C:\ESP32\esphome\` — все файлы проекта
- `C:\ESP32\REPORT_WORK_DONE.md` — этот отчёт

### Устройство:
- IP: `192.168.1.16`
- OTA: `esphome upload cat_litter_v2.yaml --device 192.168.1.16`

### GitHub:
- Репозиторий: `a4546474849-byte/esp32-cat-litter-box`
- Ветка: `add-cyber-cat`

---

## 7. Используемое железо

| Компонент | Пин | Протокол |
|-----------|-----|----------|
| Дисплей ILI9341 | CS=15, DC=2, RST=21 | SPI (VSPI: 13/12/14) |
| Тач XPT2046 | CS=33 | SPI (HSPI: 32/39/25) |
| Радар LD2410C | RX=35, TX=22 | UART 256000 |
| Серво MG995 | GPIO27 | PWM 50Hz |

---

## 8. Автомат состояний

```
IDLE → (радар детектит) → DETECTING → (5 сек) → CAT_INSIDE
CAT_INSIDE → (радар ушёл) → COOLDOWN → (30 сек) → CLEANING → (серво 2 сек) → IDLE
```

---

## 9. Важные замечания

1. **LV_COLOR_16_SWAP** уже включён по умолчанию в ESPHome LVGL
2. **Аппаратный rotation** ili9xxx **несовместим** с LVGL
3. **Touch transform** обязателен при LVGL rotation ≠ 0
4. **`auto_clear_enabled: false`** ломает LVGL — НЕ использовать
5. **`reset_pin: GPIO21`** обязателен для корректной работы дисплея
6. **PSRAM** не обнаружена на данной плате (CONFIG_SPIRAM=y не помогает)
7. **8 кадров анимации** по 320x240 не влезают в flash — нужно уменьшать
8. **AnimatedGIF в ESP-IDF**: нет Arduino.h, delay(), millis() — нужно заменять на FreeRTOS
9. **gif.inl** не распознаётся ESPHome — объединять с AnimatedGIF.cpp в один файл
10. **PowerShell redirect** кодирует вывод в UTF-16 — писать файлы из Python напрямую

---

## 10. Текущий статус

- ✅ Кот отображается на экране
- ✅ Кнопка "ВЗЛЕТ" работает
- ✅ Серво активируется при нажатии
- ✅ Радар LD2410C определяет присутствие
- ✅ Автомат состояний работает
- ✅ Анимация через animimg (8 кадров 160x120)
- ⏳ GIF-плеер через AnimatedGIF — скомпилирован, требует тестирования
