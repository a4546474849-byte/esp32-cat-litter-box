# Отчёт о проделанной работе: Smart Cat Litter на ESPHome

**Дата:** 2026-07-02
**Платформа:** ESP32-2432S028 (CYD)
**Прошивка:** ESPHome 2026.6.2 + LVGL 9.5.0

---

## 1. Что было сделано

### 1.1 Перенос проекта с Arduino на ESPHome
Проект "Smart Cat Litter Box" перенесён с Arduino/PlatformIO на ESPHome:
- Графический интерфейс с кибер-котом на LVGL
- Кнопка "ВЗЛЕТ" (очистка лотка сервоприводом)
- Автомат состояний (радар LD2410C управляет логикой)
- Навигация стрелками между страницами
- Счётчик посещений с сохранением в EEPROM

### 1.2 Анимация взлёта тарелки
При нажатии кнопки "ВЗЛЕТ" показывается анимация:
- 8 кадров покадровой анимации (нарезаны из коллажа)
- Тарелка взлетает с огненным следом
- После анимации активируется сервопривод (2 сек)
- Затем возврат к экрану с котом

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

---

## 4. Процесс прошивки

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

## 5. Расположение файлов

### WSL (рабочая папка):
- `/home/esphome/cat_litter_v2.yaml` — конфиг
- `/home/esphome/kot_320x240.png` — картинка кота
- `/home/esphome/ufo_frames/` — кадры анимации тарелки

### Windows:
- `C:\ESP32\cat_litter_v2.yaml`
- `C:\ESP32\REPORT_ESPHOME_SMART_CAT.md` — этот отчёт

### Устройство:
- IP: `192.168.1.16`
- OTA: `esphome upload cat_litter_v2.yaml --device 192.168.1.16`

### GitHub:
- Репозиторий: `a4546474849-byte/esp32-cat-litter-box`
- Ветка: `add-cyber-cat`

---

## 6. Используемое железо

| Компонент | Пин | Протокол |
|-----------|-----|----------|
| Дисплей ILI9341 | CS=15, DC=2, RST=21 | SPI (VSPI: 13/12/14) |
| Тач XPT2046 | CS=33 | SPI (HSPI: 32/39/25) |
| Радар LD2410C | RX=35, TX=22 | UART 256000 |
| Серво MG995 | GPIO27 | PWM 50Hz |

---

## 7. Автомат состояний

```
IDLE → (радар детектит) → DETECTING → (5 сек) → CAT_INSIDE
CAT_INSIDE → (радар ушёл) → COOLDOWN → (30 сек) → CLEANING → (серво 2 сек) → IDLE
```

---

## 8. Важные замечания

1. **LV_COLOR_16_SWAP** уже включён по умолчанию в ESPHome LVGL
2. **Аппаратный rotation** ili9xxx **несовместим** с LVGL
3. **Touch transform** обязателен при LVGL rotation ≠ 0
4. **`auto_clear_enabled: false`** ломает LVGL — НЕ использовать
5. **`reset_pin: GPIO21`** обязателен для корректной работы дисплея
6. **PSRAM** не обнаружена на данной плате (CONFIG_SPIRAM=y не помогает)
7. **8 кадров анимации** по 320x240 не влезают в flash — нужно уменьшать
