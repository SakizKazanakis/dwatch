#include "monolith_lvgl8_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define MW 485u
#define MH 520u
int monolith_lvgl8_create(monolith_lvgl8_view *view,lv_obj_t *root,
                          const uint8_t *rgb,size_t bytes){
    if(!view||!root||!rgb||bytes!=MW*MH*3u||view->active)return -1;
    memset(view,0,sizeof(*view));
    lv_color_t *owned=(lv_color_t*)malloc(MW*MH*sizeof(lv_color_t));
    if(!owned)return -2;
    for(size_t i=0;i<MW*MH;i++)
        owned[i]=lv_color_make(rgb[3*i],rgb[3*i+1],rgb[3*i+2]);
    view->owned_pixels=owned;
    view->descriptor.header.always_zero=0;
    view->descriptor.header.w=MW;view->descriptor.header.h=MH;
    view->descriptor.header.cf=LV_IMG_CF_TRUE_COLOR;
    view->descriptor.data_size=MW*MH*sizeof(lv_color_t);
    view->descriptor.data=(const uint8_t*)owned;
    view->image=lv_img_create(root);
    if(!view->image){free(owned);memset(view,0,sizeof(*view));return -3;}
    lv_img_set_src(view->image,&view->descriptor);
    lv_obj_align(view->image,LV_ALIGN_CENTER,0,0);
    view->time_label=lv_label_create(root);
    view->date_label=lv_label_create(root);
    if(!view->time_label||!view->date_label){monolith_lvgl8_destroy(view);return -4;}
    lv_obj_set_style_text_color(view->time_label,lv_color_hex(0xE5EFF5),0);
    lv_obj_set_style_text_color(view->date_label,lv_color_hex(0x9EB4C2),0);
    lv_obj_align(view->time_label,LV_ALIGN_CENTER,0,-12);
    lv_obj_align(view->date_label,LV_ALIGN_CENTER,0,42);
    lv_obj_move_foreground(view->time_label);
    lv_obj_move_foreground(view->date_label);
    view->active=1;
    return 0;
}
int monolith_lvgl8_set_datetime(monolith_lvgl8_view *view,
                                unsigned hour,unsigned minute,
                                unsigned day,unsigned month,unsigned year){
    if(!view||!view->active)return -1;
    if(hour>23||minute>59||day<1||day>31||month<1||month>12||year>9999)return -2;
    char t[16],d[24];
    snprintf(t,sizeof(t),"%02u:%02u",hour,minute);
    snprintf(d,sizeof(d),"%02u.%02u.%04u",day,month,year);
    lv_label_set_text(view->time_label,t);
    lv_label_set_text(view->date_label,d);
    lv_obj_align(view->time_label,LV_ALIGN_CENTER,0,-12);
    lv_obj_align(view->date_label,LV_ALIGN_CENTER,0,42);
    return 0;
}
int monolith_lvgl8_pause(monolith_lvgl8_view *view){
    if(!view||!view->active)return -1;
    lv_obj_add_flag(view->image,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->time_label,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->date_label,LV_OBJ_FLAG_HIDDEN);
    return 0;
}
int monolith_lvgl8_resume(monolith_lvgl8_view *view){
    if(!view||!view->active)return -1;
    lv_obj_clear_flag(view->image,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(view->time_label,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(view->date_label,LV_OBJ_FLAG_HIDDEN);
    return 0;
}
void monolith_lvgl8_destroy(monolith_lvgl8_view *view){
    if(!view)return;
    if(view->date_label)lv_obj_del(view->date_label);
    if(view->time_label)lv_obj_del(view->time_label);
    if(view->image)lv_obj_del(view->image);
    free(view->owned_pixels);
    memset(view,0,sizeof(*view));
}
