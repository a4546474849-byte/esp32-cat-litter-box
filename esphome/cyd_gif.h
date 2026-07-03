// =============================================================
// CYD GIF Player — интеграция AnimatedGIF + ESPHome
// Платформа: ESP32-2432S028, ILI9341 320x240
// =============================================================

#include "AnimatedGIF.h"
#include <esphome/components/display/display_buffer.h>

// Внешние данные GIF
extern const uint8_t ufo_gif_data[];
extern const unsigned int ufo_gif_data_len;

// Статические переменные для калбэка
static esphome::display::DisplayBuffer *active_display = nullptr;
static int16_t gif_offset_x = 0;
static int16_t gif_offset_y = 0;

// Callback: отрисовка строки пикселей (void, не int16_t!)
static void gifDrawCallback(GIFDRAW *pDraw) {
    if (!active_display) return;
    
    uint8_t *sLine = pDraw->pPixels;
    int iWidth = pDraw->iWidth;
    int y = pDraw->iY + pDraw->y;
    
    if (iWidth > 320) iWidth = 320;
    if (y < 0 || y >= 240) return;
    
    for (int x = 0; x < iWidth; x++) {
        uint8_t c = sLine[x];
        if (pDraw->ucHasTransparency && c == pDraw->ucTransparent) continue;
        
        uint16_t color = 0;
        if (pDraw->pPalette) {
            color = pDraw->pPalette[c];
        }
        // Свап байт для ILI9341
        color = (color >> 8) | (color << 8);
        
        active_display->draw_pixel_at(gif_offset_x + x, gif_offset_y + y, esphome::Color(color));
    }
}

class CYDGifPlayer {
public:
    CYDGifPlayer() {}
    
    void setup(esphome::display::DisplayBuffer *disp) {
        display = disp;
        active_display = disp;
        gif.begin();
    }
    
    void start() {
        if (!playing) {
            gif.open((uint8_t *)ufo_gif_data, ufo_gif_data_len, gifDrawCallback);
            playing = true;
        }
    }
    
    void stop() {
        gif.close();
        playing = false;
    }
    
    bool is_playing() { return playing; }
    
    void set_offset(int16_t x, int16_t y) {
        gif_offset_x = x;
        gif_offset_y = y;
    }
    
    void update() {
        if (!playing || !display) return;
        active_display = display;
        
        int iDelay = 0;
        if (gif.playFrame(false, &iDelay) == 0) {
            gif.close();
            gif.open((uint8_t *)ufo_gif_data, ufo_gif_data_len, gifDrawCallback);
        }
    }
    
private:
    AnimatedGIF gif;
    esphome::display::DisplayBuffer *display = nullptr;
    bool playing = false;
};

// Глобальный указатель для доступа из любой lambda
static CYDGifPlayer *global_gif_player = nullptr;
