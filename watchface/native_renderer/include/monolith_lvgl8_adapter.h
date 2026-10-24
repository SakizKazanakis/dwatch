#ifndef MONOLITH_LVGL8_ADAPTER_H
#define MONOLITH_LVGL8_ADAPTER_H
/* Explicit public LVGL v8 adapter. Not a firmware-matched HK8 ROM interface. */
#include "lvgl.h"
#include <stddef.h>
#include <stdint.h>
typedef int (*monolith_time_source)(void *ctx, unsigned *h, unsigned *m,
                                    unsigned *day, unsigned *month, unsigned *year);
typedef struct monolith_lvgl8_view {
    lv_obj_t *image;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_color_t *owned_pixels;
    lv_img_dsc_t descriptor;
    lv_timer_t *timer;
    monolith_time_source time_source;
    void *time_context;
    unsigned active;
} monolith_lvgl8_view;
/* 'rgb' is original verified eZIP decoded 485x520 RGB888. */
int monolith_lvgl8_create(monolith_lvgl8_view *view,lv_obj_t *root,
                          const uint8_t *rgb,size_t bytes);
int monolith_lvgl8_set_datetime(monolith_lvgl8_view *view,
                                unsigned hour,unsigned minute,
                                unsigned day,unsigned month,unsigned year);
int monolith_lvgl8_auto_clock(monolith_lvgl8_view *view,
                              monolith_time_source source,void *context);
int monolith_lvgl8_pause(monolith_lvgl8_view *view);
int monolith_lvgl8_resume(monolith_lvgl8_view *view);
void monolith_lvgl8_destroy(monolith_lvgl8_view *view);
#endif
