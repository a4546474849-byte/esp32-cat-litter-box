/*
 * CAT v2.0 — ESP32-2432S028 (CYD)
 * Display: ILI9341 320x240, VSPI (pins 12/13/14)
 * Touch: XPT2046, HSPI (pins 25/32/39) — отдельная шина!
 * Servo: GPIO27, Radar: UART2 RX=35 TX=22
 */
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <ESP32Servo.h>
#include <XPT2046_Touchscreen.h>
#include "cat_img.h"

#define XPT_CLK  25
#define XPT_MISO 39
#define XPT_MOSI 32
#define XPT_CS   33
#define XPT_IRQ  36

#define RADAR_RX 35
#define RADAR_TX 22
#define SERVO_PIN 27

// Тач на HSPI — отдельная шина от дисплея (VSPI)
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(XPT_CS, XPT_IRQ);
TFT_eSPI tft;
Servo servo;
HardwareSerial radarSerial(2);

// State
enum State { ST_IDLE, ST_DETECTING, ST_CAT_INSIDE, ST_COOLDOWN, ST_CLEANING };
State sysState = ST_IDLE;
uint32_t stateMs = 0, visitCount = 0;
bool manualFlush = false;
bool radarPresent = false;

// LVGL
static lv_disp_draw_buf_t draw_buf;
static lv_color_t lvBuf[320 * 10];
static lv_obj_t *lbl_visits, *lbl_clock, *btn_flush;

// Forward declaration
static void draw_cat();
static bool cat_dirty = true; // нужно перерисовать кота

void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    // Не даём LVGL писать в зону кота (y=20..204)
    lv_coord_t y1 = area->y1, y2 = area->y2;
    if (y1 <= 184 && y2 >= 0) {
        cat_dirty = true;
    } else {
        uint32_t w = area->x2 - area->x1 + 1, h = y2 - y1 + 1;
        tft.startWrite();
        tft.setAddrWindow(area->x1, y1, w, h);
        tft.pushColors((uint16_t*)color_p, w * h, true);
        tft.endWrite();
    }
    lv_disp_flush_ready(drv);
}

void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    if (ts.tirqTouched() && ts.touched()) {
        TS_Point p = ts.getPoint();
        if (p.z > 300) {
            data->point.x = constrain(map(p.x, 200, 3800, 0, 319), 0, 319);
            data->point.y = constrain(map(p.y, 200, 3800, 0, 239), 0, 239);
            data->state = LV_INDEV_STATE_PR;
            return;
        }
    }
    data->state = LV_INDEV_STATE_REL;
}

// Radar
static uint8_t rBuf[128]; static uint8_t rLen = 0;
void radar_parse() {
    while (radarSerial.available()) {
        uint8_t b = radarSerial.read();
        if (rLen == 0 && b != 0xF4) continue;
        if (rLen < sizeof(rBuf)) rBuf[rLen++] = b;
        if (rLen >= 12 && rBuf[rLen-2] == 0x55 && rBuf[rLen-1] == 0x00) {
            radarPresent = (rLen > 8) ? (rBuf[8] > 0) : false;
            rLen = 0;
        }
    }
}

// Servo
enum ServoPhase { SRV_IDLE, SRV_UP, SRV_HOLD, SRV_DOWN };
ServoPhase srvPhase = SRV_IDLE;
int srvAngle = 0; uint32_t srvMs = 0;

void servo_start() {
    servo.setPeriodHertz(50); servo.attach(SERVO_PIN, 500, 2400);
    servo.write(0); srvAngle = 0; srvPhase = SRV_UP; srvMs = millis();
}
void servo_update() {
    if (srvPhase == SRV_IDLE) return;
    uint32_t now = millis();
    if (srvPhase == SRV_UP && now - srvMs >= 17) {
        srvMs = now; servo.write(++srvAngle);
        if (srvAngle >= 90) { srvPhase = SRV_HOLD; srvMs = now; }
    } else if (srvPhase == SRV_HOLD && now - srvMs >= 2000) {
        srvPhase = SRV_DOWN; srvMs = now;
    } else if (srvPhase == SRV_DOWN && now - srvMs >= 17) {
        srvMs = now; servo.write(--srvAngle);
        if (srvAngle <= 0) { servo.detach(); srvPhase = SRV_IDLE; }
    }
}

// State machine
void state_update() {
    uint32_t now = millis();
    if (manualFlush && (sysState == ST_IDLE || sysState == ST_COOLDOWN)) {
        manualFlush = false; sysState = ST_CLEANING; stateMs = now; servo_start(); return;
    }
    switch (sysState) {
        case ST_IDLE:
            if (radarPresent) { sysState = ST_DETECTING; stateMs = now; } break;
        case ST_DETECTING:
            if (!radarPresent) { sysState = ST_IDLE; }
            else if (now - stateMs >= 5000) { sysState = ST_CAT_INSIDE; stateMs = now; } break;
        case ST_CAT_INSIDE:
            if (!radarPresent) { visitCount++; sysState = ST_COOLDOWN; stateMs = now; } break;
        case ST_COOLDOWN:
            if (radarPresent) { sysState = ST_CAT_INSIDE; stateMs = now; }
            else if (now - stateMs >= 30000) { sysState = ST_CLEANING; stateMs = now; servo_start(); } break;
        case ST_CLEANING:
            if (srvPhase == SRV_IDLE) { sysState = ST_IDLE; stateMs = now; } break;
    }
}

// Cat image — рисуем напрямую через tft, минуя LVGL (без проблем со swap)
static void draw_cat() {
    tft.startWrite();
    tft.setAddrWindow(10, 0, CAT_IMG_W, CAT_IMG_H);
    tft.pushColors((uint16_t*)cat_img, CAT_IMG_W * CAT_IMG_H, false);
    tft.endWrite();
}

// UI
static lv_obj_t *pages[3];
static uint8_t curPage = 0;

// Цвета кнопок
#define C_BTN_BG  0x3B1A00  // тёмно-коричневый
#define C_BTN_BDR 0xD4A017  // золотисто-жёлтый

static lv_obj_t* make_btn(lv_obj_t *par, const char *txt, int x, int w, lv_event_cb_t cb, void *ud) {
    lv_obj_t *b = lv_btn_create(par);
    lv_obj_set_size(b, w, 35); lv_obj_set_pos(b, x, 202);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_BTN_BG), 0);
    lv_obj_set_style_border_color(b, lv_color_hex(C_BTN_BDR), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_center(l);
    lv_obj_set_style_text_color(l, lv_color_hex(C_BTN_BDR), 0);
    return b;
}

static bool page_changed = false;

static void show_page(int p) {
    curPage = p;
    for (int i = 0; i < 3; i++) {
        if (i == p) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    page_changed = true;
}

void ui_build() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    // Нижняя полоса (всегда видна)
    lv_obj_t *bot = lv_obj_create(scr);
    lv_obj_set_size(bot, 320, 55); lv_obj_set_pos(bot, 0, 185);
    lv_obj_set_style_bg_color(bot, lv_color_hex(0x121212), 0);
    lv_obj_set_style_border_width(bot, 0, 0); lv_obj_set_style_radius(bot, 0, 0);

    // 3 кнопки
    make_btn(scr, "STAT",  4,   95, [](lv_event_t*){ show_page(1); }, NULL);
    make_btn(scr, "FLUSH", 112,  96, [](lv_event_t*){ manualFlush = true; show_page(0); }, NULL);
    btn_flush = lv_obj_get_child(scr, lv_obj_get_child_cnt(scr)-1);
    make_btn(scr, "INFO", 221,  95, [](lv_event_t*){ show_page(2); }, NULL);

    // Страница 0: кот (пустая — кот рисуется через tft напрямую)
    pages[0] = lv_obj_create(scr);
    lv_obj_set_size(pages[0], 320, 185); lv_obj_set_pos(pages[0], 0, 0);
    lv_obj_set_style_bg_opa(pages[0], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pages[0], 0, 0);
    lv_obj_set_scrollbar_mode(pages[0], LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(pages[0], LV_OBJ_FLAG_CLICKABLE);

    // Страница 1: статистика
    pages[1] = lv_obj_create(scr);
    lv_obj_set_size(pages[1], 320, 185); lv_obj_set_pos(pages[1], 0, 0);
    lv_obj_set_style_bg_color(pages[1], lv_color_hex(0x121212), 0);
    lv_obj_set_style_bg_opa(pages[1], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pages[1], 0, 0);
    lv_obj_set_scrollbar_mode(pages[1], LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(pages[1], LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *t1 = lv_label_create(pages[1]);
    lv_label_set_text(t1, "STATISTICS");
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(t1, lv_color_hex(C_BTN_BDR), 0);
    lv_obj_set_pos(t1, 70, 20);

    lbl_visits = lv_label_create(pages[1]);
    lv_label_set_text(lbl_visits, "Visits: 0");
    lv_obj_set_style_text_font(lbl_visits, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_visits, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(lbl_visits, 60, 70);

    lbl_clock = lv_label_create(pages[1]);
    lv_label_set_text(lbl_clock, "00:00:00");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(lbl_clock, 90, 120);

    // Страница 2: система
    pages[2] = lv_obj_create(scr);
    lv_obj_set_size(pages[2], 320, 185); lv_obj_set_pos(pages[2], 0, 0);
    lv_obj_set_style_bg_color(pages[2], lv_color_hex(0x121212), 0);
    lv_obj_set_style_bg_opa(pages[2], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pages[2], 0, 0);
    lv_obj_set_scrollbar_mode(pages[2], LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(pages[2], LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *t2 = lv_label_create(pages[2]);
    lv_label_set_text(t2, "SYSTEM");
    lv_obj_set_style_text_font(t2, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(t2, lv_color_hex(C_BTN_BDR), 0);
    lv_obj_set_pos(t2, 90, 20);

    lv_obj_t *inf = lv_label_create(pages[2]);
    lv_label_set_text(inf,
        "ESP32-2432S028 (CYD)\n"
        "Display: ILI9341 320x240\n"
        "Touch: XPT2046 HSPI\n"
        "Radar: HLK-LD2410C\n"
        "Servo: GPIO27");
    lv_obj_set_style_text_font(inf, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(inf, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(inf, 10, 60);
}

static void ui_timer(lv_timer_t*) {
    uint32_t s = millis()/1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", s/3600%24, s/60%60, s%60);
    lv_label_set_text(lbl_clock, buf);
    snprintf(buf, sizeof(buf), "Visits: %lu", visitCount);
    lv_label_set_text(lbl_visits, buf);
}

void setup() {
    Serial.begin(115200);
    radarSerial.begin(256000, SERIAL_8N1, RADAR_RX, RADAR_TX);

    // Тач на HSPI (отдельная шина)
    touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    // Дисплей на VSPI (через TFT_eSPI)
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);  // закрашивает все артефакты
    pinMode(21, OUTPUT); digitalWrite(21, HIGH);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, lvBuf, NULL, 320*10);
    static lv_disp_drv_t dd; lv_disp_drv_init(&dd);
    dd.hor_res=320; dd.ver_res=240; dd.flush_cb=disp_flush; dd.draw_buf=&draw_buf;
    lv_disp_drv_register(&dd);

    static lv_indev_drv_t id; lv_indev_drv_init(&id);
    id.type=LV_INDEV_TYPE_POINTER; id.read_cb=touch_read;
    lv_indev_drv_register(&id);

    ui_build();
    draw_cat();
    // Закрасить зазор между котом и нижней полосой
    tft.fillRect(0, CAT_IMG_H, 320, 205 - CAT_IMG_H, TFT_BLACK);
    lv_timer_create(ui_timer, 500, NULL);

    Serial.println("CAT v2.0 Ready");
}

void loop() {
    radar_parse();
    state_update();
    servo_update();
    lv_timer_handler();
    if (page_changed) {
        page_changed = false;
        if (curPage == 0) cat_dirty = true;
        else tft.fillRect(0, 0, 320, 185, TFT_BLACK);
    }
    if (cat_dirty && curPage == 0) { cat_dirty = false; draw_cat(); }
    delay(5);
}
