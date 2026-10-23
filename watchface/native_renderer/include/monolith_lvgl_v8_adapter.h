/* Adapter to public LVGL v8 ONLY. Not a HK8/RT-Thread OEM ABI declaration.
 * Requires LVGL v8 headers and a registered display driver.
 */
#ifndef MONOLITH_LVGL_V8_ADAPTER_H
#define MONOLITH_LVGL_V8_ADAPTER_H
#include "lvgl.h"
#include "monolith_renderer.h"
typedef struct {
    lv_obj_t *root;
    lv_obj_t *background;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_img_dsc_t image;
    monolith_resource *resource;
    unsigned minute_key,date_key;
    unsigned updates;
    int paused;
} monolith_lvgl_v8;
int monolith_lvgl_v8_open(monolith_lvgl_v8 *view,lv_obj_t *parent,
                           monolith_resource *resource);
int monolith_lvgl_v8_update(monolith_lvgl_v8 *view,unsigned hour,unsigned minute,
                             unsigned day,unsigned month,unsigned year);
int monolith_lvgl_v8_pause(monolith_lvgl_v8 *view);
int monolith_lvgl_v8_resume(monolith_lvgl_v8 *view);
void monolith_lvgl_v8_close(monolith_lvgl_v8 *view);
#endif
