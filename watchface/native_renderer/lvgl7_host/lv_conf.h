/* LVGL7 host test only; NOT proven to match HK8 firmware. */
#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdint.h>
#define LV_COLOR_DEPTH 32
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (2U * 1024U * 1024U)
#define LV_USE_IMG 1
#define LV_USE_LABEL 1
#define LV_USE_CONT 1
#define LV_USE_LOG 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#endif
