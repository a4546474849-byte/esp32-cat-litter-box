#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240
#define LV_MEM_SIZE (48U * 1024U)
#define LV_USE_PERF_MONITOR 0
#define LV_USE_LOG 0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_ARC      1
#define LV_USE_BTN      1
#define LV_USE_LABEL    1
#define LV_USE_BAR      1
#define LV_USE_CHART    1
#define LV_USE_CONT     1
#define LV_USE_CANVAS   1
#define LV_USE_ANIMIMG  0
#define LV_USE_SPINNER  1
#define LV_USE_METER    0
#define LV_USE_TABVIEW  0
#define LV_USE_WIN      0
#define LV_USE_MSGBOX   0
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 0
#define LV_USE_ROLLER   0
#define LV_USE_DROPDOWN 1
#define LV_USE_LIST     0
#define LV_USE_TABLE    0
#define LV_USE_CHECKBOX 0
#define LV_USE_SWITCH   0
#define LV_USE_SLIDER   0
#define LV_USE_IMG      1
#define LV_USE_LINE     1
#define LV_USE_OBJX_TEMPL 0
#define LV_BUILD_EXAMPLES 0

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#endif
