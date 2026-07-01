# Отчёт: Перенос проекта Smart Cat Litter на ESPHome

**Дата:** 2026-07-01  
**Платформа:** ESP32-2432S028 (CYD)  
**Прошивка:** ESPHome 2026.6.2 + LVGL 9.5.0

---

## 1. Что было сделано

Перенесён проект "Smart Cat Litter Box" с Arduino/PlatformIO на ESPHome:
- Графический интерфейс с кибер-котом
- Кнопка "ВЗЛЕТ" (очистка лотка)
- Автомат состояний (радар LD2410C + сервопривод)
- Навигация стрелками
- Статистика посещений

---

## 2. Ключевые настройки для CYD (ESP32-2432S028)

### 2.1 Дисплей ILI9341

```yaml
display:
  - platform: ili9xxx
    id: tft_display
    model: ILI9341
    spi_id: vspi_display
    cs_pin: GPIO15
    dc_pin: GPIO2
    auto_clear_enabled: false    # ВАЖНО: отключает очистку буфера
    invert_colors: false         # НЕ true! инверсия ломает цвета
    color_order: RGB             # НЕ BGR! иначе R/B каналы местами
    dimensions:
      width: 320
      height: 240
```

**Проблема с цветами:** При `invert_colors: true` и `color_order: BGR` (дефолты) все цвета отображались неправильно — чёрный фон становился серым, кибер-очки кота меняли цвет. Решение: `invert_colors: false` + `color_order: RGB`.

### 2.2 LVGL конфигурация

```yaml
lvgl:
  displays:
    - tft_display
  touchscreens:
    - my_touch
  rotation: 180          # Аппаратный поворот через MADCTL
  buffer_size: 20%
  byte_order: big_endian  # LV_COLOR_16_SWAP=1
```

**Проблема с поворотом:**
- `rotation: 90` — экран повёрнут влево
- `rotation: 270` — экран повёрнут вправо
- `rotation: 180` — правильная ориентация для CYD с USB слева

**Проблема со скроллбаром:** LVGL по умолчанию показывает полосу прокрутки при свайпе. Это выглядело как артефакт/полоса на экране. Решение: `scrollbar_mode: "OFF"`.

### 2.3 Тачскрин XPT2046

```yaml
touchscreen:
  - platform: xpt2046
    id: my_touch
    spi_id: hspi_touch
    cs_pin: GPIO33
    calibration:
      x_min: 220
      x_max: 3756
      y_min: 394
      y_max: 3749
    transform:
      swap_xy: true
      mirror_x: true
      mirror_y: true
```

**Проблема с тачем:**
- При `rotation: 180` тач-координаты инвертируются
- `swap_xy: true` — поворачивает оси X/Y
- `mirror_x: true` + `mirror_y: true` — зеркалит обе оси
- Без `transform` кнопка не работает при `rotation: 180`

### 2.4 Шрифты с кириллицей

```yaml
font:
  - file: "gfonts://Montserrat"
    id: font_24
    size: 24
    glyphsets: GF_Cyrillic_Core  # ВАЖНО для русского текста
```

**Проблема:** Дефолтный Montserrat загружает только латиницу. Для кириллицы нужен `glyphsets: GF_Cyrillic_Core`.

### 2.5 Изображения

```yaml
image:
  - file: "kot_320x240.png"
    id: img_kot
    type: RGB565
```

**Проблема с ресайзом:** ESPHome при ресайзе PNG создаёт артефакты на краях. Решение: предварительно обрезать изображение до точных 320x240 с чёрным фоном через Python/Pillow.

---

## 3. Решённые проблемы

| Проблема | Причина | Решение |
|----------|---------|---------|
| Серый вместо чёрного фона | `invert_colors: true` инвертировал все цвета | `invert_colors: false` |
| Очки кота жёлтые вместо неоновых | `color_order: BGR` менял R/B каналы | `color_order: RGB` |
| Экран повёрнут влево | `rotation: 90` | `rotation: 180` |
| Кнопка не работает | Тач-координаты не инвертированы для rotation 180 | `transform: swap_xy + mirror_x + mirror_y` |
| Кнопка "VZLET" латиницей | Montserrat без кириллицы | `glyphsets: GF_Cyrillic_Core` + текст "ВЗЛЕТ" |
| Полоса на экране | LVGL scrollbar_mode включён по дефолту | `scrollbar_mode: "OFF"` |
| Зелёный фон по бокам | дефолтный bg_color страницы | `bg_color: 0x000000` |
| Артефакты ресайза PNG | ESPHome некорректно ресайзит PNG | Предварительная обрезка через Pillow |

---

## 4. Структура проекта

```
/home/esphome/
├── cat_litter_v2.yaml          # Основной конфиг ESPHome
├── kot_320x240.png             # Обработанное изображение кота
├── kot_cyber.png               # Оригинальное изображение
├── test_lvgl_simple.yaml       # Тестовый конфиг
└── .esphome/
    └── build/
        └── smart-cat-litter/   # Собранные прошивки
```

---

## 5. Процесс прошивки

### Быстрый цикл (изменения в YAML):
```bash
# В WSL:
cd /home/esphome
esphome compile cat_litter_v2.yaml    # ~35 сек (инкрементальная)
esphome upload cat_litter_v2.yaml --device 192.168.1.16  # ~7 сек OTA
```

### Полная пересборка (если изменился framework):
```bash
rm -rf .esphome/build/smart-cat-litter
esphome compile cat_litter_v2.yaml    # ~150 сек
```

---

## 6. Используемое железо

| Компонент | Пин | Протокол |
|-----------|-----|----------|
| Дисплей ILI9341 | CS=15, DC=2, RST=21 | SPI (VSPI: 13/12/14) |
| Тач XPT2046 | CS=33 | SPI (HSPI: 32/39/25) |
| Радар LD2410C | RX=35, TX=22 | UART 256000 |
| Серво MG995 | GPIO27 | PWM 50Hz |
| Подсветка | GPIO21 | - |

---

## 7. Автомат состояний

```
IDLE → (радар детектит) → DETECTING → (5 сек) → CAT_INSIDE
CAT_INSIDE → (радар ушёл) → COOLDOWN → (30 сек) → CLEANING → (серво 2 сек) → IDLE
```

---

## 8. Важные замечания

1. **LV_COLOR_16_SWAP** уже включён по умолчанию в ESPHome LVGL (`byte_order: big_endian`)
2. **Аппаратный rotation** ili9xxx (`rotation: 90`) **несовместим** с LVGL — ESPHome запрещает
3. **Touch transform** обязателен при LVGL rotation ≠ 0
4. **auto_clear_enabled: false** критичен для статичных экранов с изображениями
