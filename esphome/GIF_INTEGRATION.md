# Интеграция AnimatedGIF в ESPHome для CYD

## Быстрый старт

### Шаг 1: Сконвертируй GIF в C-массив

```bash
# Установи Python (если нет)
pip install Pillow

# Сконвертируй GIF в C-массив
python3 gif_to_c.py твоя_гифка.gif > ufo_gif_data.h
```

### Шаг 2: Положи файлы в папку esphome

```
/home/esphome/
├── cat_litter_gif.yaml      # YAML конфиг
├── cyd_gif.h                # Заголовок с классом CYDGifPlayer
└── ufo_gif_data.h           # C-массив с данными GIF
```

### Шаг 3: Скомпилируй и прошей

```bash
cd /home/esphome
esphome compile cat_litter_gif.yaml
esphome upload cat_litter_gif.yaml --device 192.168.1.16
```

---

## Как это работает

### Архитектура

```
┌─────────────────────────────────────────┐
│  Flash память ESP32                     │
│  ┌───────────────────────────────────┐  │
│  │ ufo_gif_data[] (PROGMEM)         │  │
│  │ GIF байты хранятся здесь         │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────────┐
│  AnimatedGIF Library (bitbank2)         │
│  - Декодирует GIF на лету              │
│  - Не загружает весь кадр в RAM        │
│  - Строками передаёт в callback        │
└─────────────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────────┐
│  gifDrawCallback()                     │
│  - Конвертирует палитру → RGB565       │
│  - Рисует строку пикселей напрямую     │
│  - В ESPHome DisplayBuffer              │
└─────────────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────────┐
│  ILI9341 (320x240)                     │
│  Пиксели сразу на экране              │
└─────────────────────────────────────────┘
```

### Преимущества перед animimg

| Параметр | AnimatedGIF | animimg (8 кадров) |
|----------|-------------|---------------------|
| RAM | ~2KB (декодер) | ~1.2MB (все кадры) |
| Flash | Размер GIF файла | 8 × 320 × 240 × 2 = 1.2MB |
| Скорость | 30 FPS | 5-10 FPS |
| Качество | Оригинальное | Сжатие при компиляции |

---

## Ограничения

1. **Размер GIF**: GIF должен быть < 200KB для надёжной работы
2. **Разрешение**: Рекомендуется <= 320x240
3. **Палитра**: Лучше GIF с 256 цветами (标准)
4. **Прозрачность**: Поддерживается, но замедляет отрисовку

---

## Пример YAML для использования GIF

```yaml
# В блоке display: — lambda вызывается каждый кадр
display:
  - platform: ili9xxx
    ...
    lambda: |-
      static CYDGifPlayer *player = nullptr;
      if (player == nullptr) {
          player = new CYDGifPlayer();
          CYDGifPlayer::current_instance = player;
          player->setup(it);
          player->set_offset(0, 0);  // Позиция на экране
      }
      player->update();  // Декодирует и рисует следующий кадр

# Запуск GIF по кнопке
script:
  - id: play_ufo
    then:
      - lambda: |-
          CYDGifPlayer::current_instance->start();
      - delay: 3s
      - lambda: |-
          CYDGifPlayer::current_instance->stop();
```
