/* Public LVGL 8 standalone watchface interface: real LVGL objects, no mocks.
 * Original image pixels come from independently decoded clock_bg.bin.
 * NOT a firmware-matched HK8 installable ELF.
 */
#include "monolith_lvgl_v8_adapter.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int monolith_lvgl_v8_open(monolith_lvgl_v8 *view,lv_obj_t *parent,
                           monolith_resource *r) {
    if(!view||!parent||!r||!r->rgb)return MONOLITH_INVALID;
    memset(view,0,sizeof(*view));
    view->resource=r;
    view->root=lv_obj_create(parent);
    if(!view->root)return MONOLITH_NOMEM;
    lv_obj_set_size(view->root,r->width,r->height);
    lv_obj_clear_flag(view->root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(view->root,0,0);
    lv_obj_set_style_border_width(view->root,0,0);
    lv_obj_set_style_pad_all(view->root,0,0);
    lv_obj_set_style_bg_color(view->root,lv_color_black(),0);
    lv_obj_align(view->root,LV_ALIGN_CENTER,0,0);

    /* LVGL 8.3's stock software decoder DOES NOT draw LV_IMG_CF_RGB888:
     * its supported true-colour path expects LV_COLOR_DEPTH=32 packed
     * blue, green, red, alpha in memory. Convert actual eZIP RGB pixel data
     * once, retain ownership until after lv_obj_del().
     */
#if LV_COLOR_DEPTH != 32
    monolith_lvgl_v8_close(view);
    return MONOLITH_UNSUPPORTED;
#else
    view->lvgl_pixel_bytes=((size_t)r->width*r->height)*4u;
    view->lvgl_pixels=(uint8_t*)malloc(view->lvgl_pixel_bytes);
    if(!view->lvgl_pixels){
        monolith_lvgl_v8_close(view);return MONOLITH_NOMEM;
    }
    for(size_t i=0;i<(size_t)r->width*r->height;i++){
        view->lvgl_pixels[i*4u]=r->rgb[i*3u+2u];
        view->lvgl_pixels[i*4u+1u]=r->rgb[i*3u+1u];
        view->lvgl_pixels[i*4u+2u]=r->rgb[i*3u];
        view->lvgl_pixels[i*4u+3u]=255;
    }
    view->image.header.always_zero=0;
    view->image.header.w=r->width;
    view->image.header.h=r->height;
    view->image.header.cf=LV_IMG_CF_TRUE_COLOR;
    view->image.data_size=(uint32_t)view->lvgl_pixel_bytes;
    view->image.data=view->lvgl_pixels;
#endif
    view->background=lv_img_create(view->root);
    if(!view->background){monolith_lvgl_v8_close(view);return MONOLITH_NOMEM;}
    lv_img_set_src(view->background,&view->image);
    lv_obj_align(view->background,LV_ALIGN_CENTER,0,0);

    view->time_label=lv_label_create(view->root);
    view->date_label=lv_label_create(view->root);
    if(!view->time_label||!view->date_label){
        monolith_lvgl_v8_close(view);return MONOLITH_NOMEM;
    }
    lv_obj_set_style_text_color(view->time_label,lv_color_hex(0xe8eff2),0);
    lv_obj_set_style_text_color(view->date_label,lv_color_hex(0x91a5b2),0);
    lv_obj_set_style_bg_opa(view->time_label,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_opa(view->date_label,LV_OPA_TRANSP,0);
    view->minute_key=~0u;view->date_key=~0u;view->paused=0;
    return MONOLITH_OK;
}
int monolith_lvgl_v8_update(monolith_lvgl_v8 *v,unsigned h,unsigned min,
                             unsigned d,unsigned mo,unsigned y) {
    if(!v||!v->root)return MONOLITH_STATE;
    if(v->paused)return MONOLITH_OK;
    if(h>23||min>59||d==0||d>31||mo<1||mo>12||y<1970||y>2099)
        return MONOLITH_INVALID;
    unsigned mk=h*60u+min, dk=y*512u+mo*32u+d;
    if(mk==v->minute_key&&dk==v->date_key)return MONOLITH_OK;
    char time[9],date[16];
    snprintf(time,sizeof(time),"%02u:%02u",h,min);
    snprintf(date,sizeof(date),"%02u.%02u.%04u",d,mo,y);
    lv_label_set_text(v->time_label,time);
    lv_label_set_text(v->date_label,date);
    lv_obj_align(v->time_label,LV_ALIGN_TOP_MID,0,240);
    lv_obj_align(v->date_label,LV_ALIGN_TOP_MID,0,301);
    v->minute_key=mk;v->date_key=dk;v->updates++;
    return MONOLITH_OK;
}
int monolith_lvgl_v8_pause(monolith_lvgl_v8 *v){
    if(!v||!v->root)return MONOLITH_STATE;
    v->paused=1;return MONOLITH_OK;
}
int monolith_lvgl_v8_resume(monolith_lvgl_v8 *v){
    if(!v||!v->root)return MONOLITH_STATE;
    v->paused=0;v->minute_key=~0u;v->date_key=~0u;
    return MONOLITH_OK;
}
void monolith_lvgl_v8_close(monolith_lvgl_v8 *v){
    if(!v)return;
    if(v->root)lv_obj_del(v->root);
    free(v->lvgl_pixels);
    memset(v,0,sizeof(*v));
}
