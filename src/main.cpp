/*
 * ============================================================
 *  CAT v1.1  —  ESP32-2432S028 (CYD)
 *  Board  : Classic ESP32-D0WD-V3
 *  Screen : ILI9341 320x240 Landscape, TFT_eSPI
 *  Touch  : XPT2046
 *  Radar  : HLK-LD2410C  (UART2, RX=GPIO35, TX=GPIO22)
 *  Servo  : GPIO 27
 * ============================================================
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <ESP32Servo.h>
#include <math.h>
#include "cat_img.h"

// ============================================================
//  SECTION 1: HARDWARE PINS & TIMING CONSTANTS
// ============================================================
#define RADAR_RX_PIN          35    // ESP RX  <- Radar TX
#define RADAR_TX_PIN          22    // ESP TX  -> Radar RX
#define RADAR_BAUD            256000

#define SERVO_PIN             27
#define SERVO_MAX_ANGLE       90
#define SERVO_HOLD_TIME       2000  // ms — pause at top

#define FALSE_TRIGGER_TIMEOUT 5000  // ms — confirm cat entry
#define CAT_LEAVE_TIMEOUT     30000 // ms — delay before clean

// Touch calibration for CYD XPT2046 (landscape)
#define TOUCH_X_MIN  200
#define TOUCH_X_MAX  3700
#define TOUCH_Y_MIN  240
#define TOUCH_Y_MAX  3800

// ============================================================
//  SECTION 2: COLOR PALETTE  "Dark Cyber"
// ============================================================
#define C_BG        0x121212u
#define C_BAR       0x1E1E1Eu
#define C_MINT      0x10B981u  // IDLE / OK
#define C_CORAL     0xF43F5Eu  // DANGER / CLEANING
#define C_AMBER     0xF59E0Bu  // DETECTING / INSIDE
#define C_WHITE     0xFFFFFFu
#define C_GREY      0x888888u
#define C_DARKGREY  0x333333u

static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) |
           ((uint16_t)(g & 0xFC) << 3) |
           (b >> 3);
}

// ============================================================
//  SECTION 3: STATE MACHINE
// ============================================================
enum State { ST_IDLE, ST_DETECTING, ST_CAT_INSIDE, ST_COOLDOWN, ST_CLEANING };

State    sysState     = ST_IDLE;
uint32_t stateEnterMs = 0;
uint32_t visitCount   = 0;
uint32_t visitStartMs = 0;
bool     manualFlush  = false;

// Last 5 visit durations (seconds) for chart
uint32_t cleanHistory[5] = {0};
uint8_t  histIdx          = 0;

const char* state_str() {
    switch (sysState) {
        case ST_IDLE:       return "IDLE \xE2\x80\x94 Ожидание";
        case ST_DETECTING:  return "Обнаружение...";
        case ST_CAT_INSIDE: return "Кот внутри";
        case ST_COOLDOWN:   return "Ожидание уборки";
        case ST_CLEANING:   return "УБОРКА";
        default:            return "---";
    }
}

uint32_t state_color_hex() {
    switch (sysState) {
        case ST_IDLE:                  return C_MINT;
        case ST_DETECTING:
        case ST_CAT_INSIDE:
        case ST_COOLDOWN:              return C_AMBER;
        case ST_CLEANING:              return C_CORAL;
        default:                       return C_WHITE;
    }
}

// ============================================================
//  SECTION 4: SERVO (smooth, non-blocking)
// ============================================================
Servo servoMotor;

enum ServoPhase { SRV_IDLE, SRV_UP, SRV_HOLD, SRV_DOWN };
ServoPhase servoPhase = SRV_IDLE;
int        servoAngle = 0;
uint32_t   servoMs    = 0;

void servo_start() {
    servoMotor.setPeriodHertz(50);
    servoMotor.attach(SERVO_PIN, 500, 2400);
    servoMotor.write(0);
    servoAngle = 0;
    servoPhase = SRV_UP;
    servoMs    = millis();
}

void servo_update() {
    if (servoPhase == SRV_IDLE) return;
    uint32_t now = millis();
    switch (servoPhase) {
        case SRV_UP:
            if (now - servoMs >= 17) {           // ~60 fps ramp
                servoMs = now;
                servoMotor.write(++servoAngle);
                if (servoAngle >= SERVO_MAX_ANGLE) {
                    servoPhase = SRV_HOLD;
                    servoMs    = now;
                }
            }
            break;
        case SRV_HOLD:
            if (now - servoMs >= SERVO_HOLD_TIME) {
                servoPhase = SRV_DOWN;
                servoMs    = now;
            }
            break;
        case SRV_DOWN:
            if (now - servoMs >= 17) {
                servoMs = now;
                servoMotor.write(--servoAngle);
                if (servoAngle <= 0) {
                    servoMotor.detach();
                    servoPhase = SRV_IDLE;
                }
            }
            break;
        default: break;
    }
}

// ============================================================
//  SECTION 5: RADAR  (HLK-LD2410C simple frame parser)
// ============================================================
HardwareSerial radarSerial(2);
bool    radarPresent  = false;
uint8_t radarStrength = 0;

static uint8_t rBuf[128];
static uint8_t rLen = 0;

void radar_parse() {
    while (radarSerial.available()) {
        uint8_t b = radarSerial.read();
        if (rLen == 0 && b != 0xF4) continue;
        if (rLen < sizeof(rBuf)) rBuf[rLen++] = b;
        if (rLen >= 12 && rBuf[rLen-2] == 0x55 && rBuf[rLen-1] == 0x00) {
            uint8_t state = (rLen > 8)  ? rBuf[8]  : 0;
            uint8_t ms    = (rLen > 9)  ? rBuf[9]  : 0;
            uint8_t ss    = (rLen > 11) ? rBuf[11] : 0;
            radarPresent  = (state > 0);
            radarStrength = max(ms, ss);
            rLen = 0;
        }
    }
}

// ============================================================
//  SECTION 6: DISPLAY / LVGL / SPRITE
// ============================================================
static uint8_t curPage = 0;  // current UI page (used by animation dispatch)
TFT_eSPI   tft;
TFT_eSprite sprite(&tft);   // double-buffer for animation

#define DISP_BUF_LINES 10
static lv_disp_draw_buf_t draw_buf;
static lv_color_t         lvBuf[LV_HOR_RES_MAX * DISP_BUF_LINES];

void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)color_p, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty, 40)) {
        data->point.x = constrain((int32_t)map(tx, TOUCH_X_MIN, TOUCH_X_MAX, 0, 319), 0, 319);
        data->point.y = constrain((int32_t)map(ty, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 239), 0, 239);
        data->state   = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ============================================================
//  SECTION 7: ANIMATION  (real photo + overlay effects)
// ============================================================
#define SPR_W   300
#define SPR_H   185
#define SPR_X   10
#define SPR_Y   20
#define GEAR_W  140
#define GEAR_H  130

static float animPhase = 0.0f;
static bool  catImgDrawn = false;  // photo only needs one push unless state changed
static State lastDrawnState = ST_IDLE;

// Draw rotating gear overlay on sprite for CLEANING state
static void draw_gear_overlay(float ph) {
    uint16_t col = rgb(244, 63, 94);
    int cx = 70, cy = 55, R = 28, r = 20, ri = 11;
    for (int t = 0; t < 8; t++) {
        float a0 = ph + t * 2.0f * (float)M_PI / 8;
        float a1 = a0 + 0.25f;
        int x0 = cx+(int)(r*cosf(a0)), y0 = cy+(int)(r*sinf(a0));
        int x1 = cx+(int)(R*cosf(a0)), y1 = cy+(int)(R*sinf(a0));
        int x2 = cx+(int)(R*cosf(a1)), y2 = cy+(int)(R*sinf(a1));
        int x3 = cx+(int)(r*cosf(a1)), y3 = cy+(int)(r*sinf(a1));
        sprite.fillTriangle(x0,y0,x1,y1,x2,y2, col);
        sprite.fillTriangle(x0,y0,x2,y2,x3,y3, col);
    }
    for (int y = -r; y <= r; y++) {
        int hw = (int)sqrtf((float)(r*r-y*y));
        sprite.drawFastHLine(cx-hw, cy+y, 2*hw+1, col);
    }
    // hole
    for (int y = -ri; y <= ri; y++) {
        int hw = (int)sqrtf((float)(ri*ri-y*y));
        sprite.drawFastHLine(cx-hw, cy+y, 2*hw+1, rgb(20,20,20));
    }
    sprite.setTextColor(col);
    sprite.setTextSize(1);
    sprite.setCursor(14, 112); sprite.print("ИДЕТ УБОРКА...");
}

// Amber border overlay for alert states
static void draw_alert_border(float ph) {
    uint16_t col = rgb(245,158,11);
    uint8_t  a   = (uint8_t)(128 + 127 * sinf(ph * 3.0f));
    if (a > 180) {  // pulsing border
        sprite.drawRect(0, 0, SPR_W, SPR_H, col);
        sprite.drawRect(1, 1, SPR_W-2, SPR_H-2, col);
    }
}

// Zzz overlay for idle
static void draw_zzz(float ph) {
    uint8_t za = (uint8_t)(80 + 80 * sinf(ph * 0.4f));
    sprite.setTextColor(rgb(za, za, 40));
    sprite.setTextSize(1);
    sprite.setCursor(112, 18); sprite.print("z");
    sprite.setCursor(118, 11); sprite.print("z");
    sprite.setCursor(124,  4); sprite.print("Z");
}

void update_animation() {
    if (curPage != 0) return;
    bool stateChanged = (sysState != lastDrawnState);
    lastDrawnState = sysState;

    if (sysState == ST_CLEANING) {
        animPhase += 0.06f;
        // Draw photo directly to TFT (no sprite color conversion)
        tft.startWrite();
        tft.setAddrWindow(SPR_X, SPR_Y, SPR_W, SPR_H);
        tft.pushColors((uint16_t*)cat_img, SPR_W * SPR_H, false);
        tft.endWrite();
        // Gear overlay via sprite on top
        sprite.fillSprite(0x0000);
        draw_gear_overlay(animPhase);
        sprite.pushSprite(SPR_X + 80, SPR_Y + 27);
    } else if (stateChanged) {
        tft.startWrite();
        tft.setAddrWindow(SPR_X, SPR_Y, SPR_W, SPR_H);
        tft.pushColors((uint16_t*)cat_img, SPR_W * SPR_H, false);
        tft.endWrite();
    }
}

// ============================================================
//  SECTION 8: LVGL UI
// ============================================================

static lv_obj_t *pages[3];

// Dashboard
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_clock;
static lv_obj_t *lbl_radar_icon;
static lv_obj_t *btn_flush;
static lv_obj_t *lbl_flush;
static lv_obj_t *lbl_visits;    // visit counter on dashboard

// Stats
static lv_obj_t *chart;
static lv_chart_series_t *chart_ser;
static lv_obj_t *lbl_total;

// System
static lv_obj_t *lbl_uptime;
static lv_obj_t *lbl_radar_pct;

// Styles
static lv_style_t st_page, st_btn_nav, st_btn_flush, st_btn_flush_pr, st_btn_nav_pr;

void styles_init() {
    lv_style_init(&st_page);
    lv_style_set_bg_color(&st_page,   lv_color_hex(C_BG));
    lv_style_set_bg_opa(&st_page,     LV_OPA_COVER);
    lv_style_set_border_width(&st_page, 0);
    lv_style_set_pad_all(&st_page,    0);
    lv_style_set_radius(&st_page,     0);

    lv_style_init(&st_btn_nav);
    lv_style_set_bg_color(&st_btn_nav,   lv_color_hex(0x2a2a2a));
    lv_style_set_bg_opa(&st_btn_nav,     LV_OPA_COVER);
    lv_style_set_border_width(&st_btn_nav, 0);
    lv_style_set_radius(&st_btn_nav,     6);
    lv_style_set_text_color(&st_btn_nav, lv_color_hex(C_WHITE));

    lv_style_init(&st_btn_nav_pr);
    lv_style_set_bg_color(&st_btn_nav_pr, lv_color_hex(0x555555));

    lv_style_init(&st_btn_flush);
    lv_style_set_bg_color(&st_btn_flush,   lv_color_hex(C_MINT));
    lv_style_set_bg_opa(&st_btn_flush,     LV_OPA_COVER);
    lv_style_set_border_width(&st_btn_flush, 0);
    lv_style_set_radius(&st_btn_flush,     6);
    lv_style_set_text_color(&st_btn_flush, lv_color_hex(C_WHITE));

    lv_style_init(&st_btn_flush_pr);
    lv_style_set_bg_color(&st_btn_flush_pr, lv_color_hex(0x059669));
}

// Helper
static lv_obj_t* mklabel(lv_obj_t *p, const char *t, const lv_font_t *f, uint32_t col, int x, int y) {
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void page_switch(int dir) {
    curPage = (uint8_t)((curPage + 3 + dir) % 3);
    for (int i = 0; i < 3; i++) {
        if (i == curPage) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else              lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void cb_prev(lv_event_t *e)  { page_switch(-1); }
static void cb_next(lv_event_t *e)  { page_switch(+1); }
static void cb_flush(lv_event_t *e) {
    if (sysState == ST_IDLE || sysState == ST_COOLDOWN) manualFlush = true;
}

void ui_build() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // ── STATUS BAR ───────────────────────────────────────────
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 320, 20);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_add_style(bar, &st_page, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    lbl_clock = mklabel(bar, "00:00:00", &lv_font_montserrat_14, C_WHITE,    4,   2);
    mklabel(bar, "CAT v1.1",              &lv_font_montserrat_12, 0x555555, 100,  2);
    lbl_radar_icon = mklabel(bar, LV_SYMBOL_WIFI, &lv_font_montserrat_14, C_MINT, 294, 2);

    // ── NAV BAR ──────────────────────────────────────────────
    lv_obj_t *nav = lv_obj_create(scr);
    lv_obj_set_size(nav, 320, 35);
    lv_obj_set_pos(nav, 0, 205);
    lv_obj_add_style(nav, &st_page, 0);
    lv_obj_set_style_bg_color(nav, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);

    auto make_nav_btn = [](lv_obj_t *nav, int x, int w, lv_event_cb_t cb, const char *icon) {
        lv_obj_t *b = lv_btn_create(nav);
        lv_obj_set_size(b, w, 30);
        lv_obj_set_pos(b, x, 2);
        lv_obj_add_style(b, &st_btn_nav, 0);
        lv_obj_add_style(b, &st_btn_nav_pr, LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, icon);
        lv_obj_center(l);
    };
    make_nav_btn(nav, 4,  50, cb_prev, LV_SYMBOL_LEFT);
    make_nav_btn(nav, 58, 50, cb_next, LV_SYMBOL_RIGHT);

    btn_flush = lv_btn_create(nav);
    lv_obj_set_size(btn_flush, 155, 30);
    lv_obj_set_pos(btn_flush, 158, 2);
    lv_obj_add_style(btn_flush, &st_btn_flush, 0);
    lv_obj_add_style(btn_flush, &st_btn_flush_pr, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_flush, cb_flush, LV_EVENT_CLICKED, NULL);
    lbl_flush = lv_label_create(btn_flush);
    lv_label_set_text(lbl_flush, LV_SYMBOL_REFRESH "  СМЫВ");
    lv_obj_center(lbl_flush);

    // ── PAGE 0: DASHBOARD ────────────────────────────────────
    pages[0] = lv_obj_create(scr);
    lv_obj_set_size(pages[0], 320, 185);
    lv_obj_set_pos(pages[0], 0, 20);
    lv_obj_add_style(pages[0], &st_page, 0);
    lv_obj_set_scrollbar_mode(pages[0], LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(pages[0], lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(pages[0], LV_OPA_COVER, 0);

    // Status and visits shown in status bar area — no widgets overlap the photo
    lbl_status = mklabel(pages[0], "IDLE", &lv_font_montserrat_14, C_MINT, 2, 168);
    lbl_visits = mklabel(pages[0], "0", &lv_font_montserrat_14, C_WHITE, 308, 168);

    // ── PAGE 1: STATS ────────────────────────────────────────
    pages[1] = lv_obj_create(scr);
    lv_obj_set_size(pages[1], 320, 185);
    lv_obj_set_pos(pages[1], 0, 20);
    lv_obj_add_style(pages[1], &st_page, 0);
    lv_obj_set_scrollbar_mode(pages[1], LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(pages[1], LV_OBJ_FLAG_HIDDEN);

    mklabel(pages[1], "СТАТИСТИКА", &lv_font_montserrat_16, C_MINT, 10, 4);

    chart = lv_chart_create(pages[1]);
    lv_obj_set_size(chart, 292, 105);
    lv_obj_set_pos(chart, 14, 30);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 120);
    lv_chart_set_point_count(chart, 5);
    lv_obj_set_style_bg_color(chart,      lv_color_hex(C_BAR),      0);
    lv_obj_set_style_bg_opa(chart,        LV_OPA_COVER,              0);
    lv_obj_set_style_border_color(chart,  lv_color_hex(C_DARKGREY),  0);
    lv_obj_set_style_border_width(chart,  1,                         0);
    lv_obj_set_style_radius(chart,        6,                         0);
    chart_ser = lv_chart_add_series(chart, lv_color_hex(C_MINT), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 5; i++) lv_chart_set_next_value(chart, chart_ser, 0);

    lbl_total = mklabel(pages[1], "Всего уборок: 0", &lv_font_montserrat_14, C_GREY, 14, 145);

    // ── PAGE 2: SYSTEM ───────────────────────────────────────
    pages[2] = lv_obj_create(scr);
    lv_obj_set_size(pages[2], 320, 185);
    lv_obj_set_pos(pages[2], 0, 20);
    lv_obj_add_style(pages[2], &st_page, 0);
    lv_obj_set_scrollbar_mode(pages[2], LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(pages[2], LV_OBJ_FLAG_HIDDEN);

    // ASCII logo
    mklabel(pages[2],
        "  /\\_/\\ \n"
        " ( ^.^ )\n"
        "  > ~ < \n"
        "  SMART \n"
        " LITTER ",
        &lv_font_montserrat_14, C_MINT, 8, 6);

    mklabel(pages[2], "CAT v1.1",                 &lv_font_montserrat_12, 0x555555, 128, 8);
    mklabel(pages[2], "ESP32-2432S028 (CYD)",   &lv_font_montserrat_12, 0x444444, 128, 24);

    lbl_uptime     = mklabel(pages[2], "Аптайм: 0 мин", &lv_font_montserrat_14, C_WHITE,  128, 50);
    lbl_radar_pct  = mklabel(pages[2], "Радар: ---",     &lv_font_montserrat_14, C_WHITE,  128, 74);

    mklabel(pages[2],
        "Radar : HLK-LD2410C\n"
        "Servo : GPIO 27\n"
        "Screen: ILI9341 320x240",
        &lv_font_montserrat_12, 0x444444, 128, 106);
}

// ============================================================
//  SECTION 9: LVGL TIMER CALLBACKS
// ============================================================

// UI refresh — 2 Hz
static void ui_timer_cb(lv_timer_t *) {
    // Clock (uptime-based)
    uint32_t s = millis() / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", s/3600%24, s/60%60, s%60);
    lv_label_set_text(lbl_clock, buf);

    // Radar icon color
    lv_obj_set_style_text_color(lbl_radar_icon,
        radarPresent ? lv_color_hex(C_CORAL) : lv_color_hex(C_MINT), 0);

    // Status & flush button color
    lv_label_set_text(lbl_status, state_str());
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(state_color_hex()), 0);
    lv_obj_set_style_bg_color(btn_flush,
        lv_color_hex(sysState == ST_CLEANING ? C_CORAL : C_MINT), 0);

    // Visit count on dashboard
    snprintf(buf, sizeof(buf), "%lu", visitCount);
    lv_label_set_text(lbl_visits, buf);

    // Stats page
    if (curPage == 1) {
        lv_chart_set_all_value(chart, chart_ser, LV_CHART_POINT_NONE);
        for (int i = 0; i < 5; i++) {
            uint8_t idx = (uint8_t)((histIdx + i) % 5);
            lv_chart_set_next_value(chart, chart_ser, (lv_coord_t)cleanHistory[idx]);
        }
        lv_chart_refresh(chart);
        snprintf(buf, sizeof(buf), "Всего уборок: %lu", visitCount);
        lv_label_set_text(lbl_total, buf);
    }

    // System page
    if (curPage == 2) {
        snprintf(buf, sizeof(buf), "Аптайм: %lu мин", millis() / 60000UL);
        lv_label_set_text(lbl_uptime, buf);
        snprintf(buf, sizeof(buf), "Радар: %u%%", (unsigned)constrain(radarStrength, 0, 100));
        lv_label_set_text(lbl_radar_pct, buf);
    }
}

// Animation — ~30 fps
static void anim_timer_cb(lv_timer_t *) {
    update_animation();
}

// ============================================================
//  SECTION 10: STATE MACHINE (millis-based, no delay)
// ============================================================
void state_machine_update() {
    uint32_t now = millis();

    if (manualFlush && (sysState == ST_IDLE || sysState == ST_COOLDOWN)) {
        manualFlush  = false;
        sysState     = ST_CLEANING;
        stateEnterMs = now;
        servo_start();
        return;
    }

    switch (sysState) {
        case ST_IDLE:
            if (radarPresent) { sysState = ST_DETECTING; stateEnterMs = now; }
            break;

        case ST_DETECTING:
            if (!radarPresent) { sysState = ST_IDLE; stateEnterMs = now; }
            else if (now - stateEnterMs >= FALSE_TRIGGER_TIMEOUT) {
                sysState = ST_CAT_INSIDE;
                visitStartMs = stateEnterMs = now;
            }
            break;

        case ST_CAT_INSIDE:
            if (!radarPresent) {
                uint32_t dur = (now - visitStartMs) / 1000;
                cleanHistory[histIdx % 5] = dur;
                histIdx++;
                visitCount++;
                sysState = ST_COOLDOWN;
                stateEnterMs = now;
            }
            break;

        case ST_COOLDOWN:
            if (radarPresent) {
                sysState = ST_CAT_INSIDE;
                visitStartMs = stateEnterMs = now;
            } else if (now - stateEnterMs >= CAT_LEAVE_TIMEOUT) {
                sysState = ST_CLEANING;
                stateEnterMs = now;
                servo_start();
            }
            break;

        case ST_CLEANING:
            if (servoPhase == SRV_IDLE) { sysState = ST_IDLE; stateEnterMs = now; }
            break;
    }
}

// ============================================================
//  SETUP & LOOP
// ============================================================
void setup() {
    Serial.begin(115200);

    radarSerial.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);

    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    sprite.createSprite(GEAR_W, GEAR_H);
    sprite.setColorDepth(16);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, lvBuf, nullptr, LV_HOR_RES_MAX * DISP_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 320;
    disp_drv.ver_res  = 240;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_t *disp   = lv_disp_drv_register(&disp_drv);

    // Use minimal theme — default theme overrides bg colors
    lv_theme_t *th = lv_theme_basic_init(disp);
    lv_disp_set_theme(disp, th);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    styles_init();
    ui_build();

    lv_timer_create(ui_timer_cb,  500, nullptr);  // UI refresh 2 Hz
    lv_timer_create(anim_timer_cb, 33, nullptr);  // animation ~30 fps

    Serial.println("CAT v1.1 — Ready");
}

void loop() {
    radar_parse();
    state_machine_update();
    servo_update();
    lv_timer_handler();
    delay(5);
}
