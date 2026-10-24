#ifndef MONOLITH_LVGL7_ADAPTER_H
#define MONOLITH_LVGL7_ADAPTER_H
/* Public LVGL 7 adapter, not a firmware-matched HK8 binary interface. */
#include "lvgl.h"
#include <stddef.h>
#include <stdint.h>
typedef int (*monolith_lvgl7_time_source)(void *,unsigned *,unsigned *,unsigned *,
                                          unsigned *,unsigned *);
typedef struct monolith_lvgl7_view {
    lv_obj_t *image,*time_label,*date_label;
    lv_color_t *owned_pixels;
    lv_img_dsc_t descriptor;
    lv_task_t *task;
    monolith_lvgl7_time_source source;
    void *source_ctx;
    unsigned active;
} monolith_lvgl7_view;
int monolith_lvgl7_create(monolith_lvgl7_view *view,lv_obj_t *root,
                          const uint8_t *rgb,size_t bytes);
int monolith_lvgl7_set_datetime(monolith_lvgl7_view *view,
                                unsigned hour,unsigned minute,
                                unsigned day,unsigned month,unsigned year);
int monolith_lvgl7_auto_clock(monolith_lvgl7_view *view,
                              monolith_lvgl7_time_source source,void *context);
int monolith_lvgl7_pause(monolith_lvgl7_view *view);
int monolith_lvgl7_resume(monolith_lvgl7_view *view);
void monolith_lvgl7_destroy(monolith_lvgl7_view *view);
#endif
