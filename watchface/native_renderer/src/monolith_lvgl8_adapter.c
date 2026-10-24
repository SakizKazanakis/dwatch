#include "monolith_lvgl8_adapter.h"
#include "monolith_renderer.h"
#if defined(MONOLITH_FREESTANDING_ARM)
extern void *malloc(size_t);
extern void free(void *);
extern void *memset(void *, int, size_t);
#else
#include <stdlib.h>
#include <string.h>
#endif
#define MW 485u
#define MH 520u
static int monolith_lvgl8_finish(monolith_lvgl8_view *view,lv_obj_t *root);
int monolith_lvgl8_create(monolith_lvgl8_view *view,lv_obj_t *root,
                          const uint8_t *rgb,size_t bytes){
    if(!view||!root||!rgb||bytes!=MW*MH*3u||view->active)return -1;
    memset(view,0,sizeof(*view));
    lv_color_t *owned=(lv_color_t*)malloc(MW*MH*sizeof(lv_color_t));
    if(!owned)return -2;
    for(size_t i=0;i<MW*MH;i++)
        owned[i]=lv_color_make(rgb[3*i],rgb[3*i+1],rgb[3*i+2]);
    view->owned_pixels=owned;
    return monolith_lvgl8_finish(view,root);
}
static int monolith_lvgl8_finish(monolith_lvgl8_view *view,lv_obj_t *root){
    lv_color_t *owned=view->owned_pixels;
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
typedef struct {
    lv_color_t *pixels;
    unsigned rows;
} monolith_lvgl8_stream;
static int monolith_lvgl8_stream_row(void *context,unsigned row,const uint8_t *rgb,size_t bytes){
    monolith_lvgl8_stream *state=(monolith_lvgl8_stream*)context;
    if(!state || row!=state->rows || bytes!=MW*3u || row>=MH)return -1;
    lv_color_t *dst=state->pixels+(size_t)row*MW;
    for(unsigned x=0;x<MW;x++)
        dst[x]=lv_color_make(rgb[x*3u],rgb[x*3u+1u],rgb[x*3u+2u]);
    state->rows++;
    return 0;
}
int monolith_lvgl8_create_from_ezip(monolith_lvgl8_view *view,lv_obj_t *root,
                                    const uint8_t *ezip,size_t bytes){
    if(!view||!root||!ezip||view->active)return -1;
    memset(view,0,sizeof(*view));
    lv_color_t *pixels=(lv_color_t*)malloc(MW*MH*sizeof(lv_color_t));
    if(!pixels)return -2;
    monolith_lvgl8_stream state={pixels,0};
    if(monolith_resource_decode_scanlines(ezip,bytes,monolith_lvgl8_stream_row,&state)!=MONOLITH_OK
            || state.rows!=MH){
        free(pixels);
        return -3;
    }
    view->owned_pixels=pixels;
    return monolith_lvgl8_finish(view,root);
}
int monolith_lvgl8_set_datetime(monolith_lvgl8_view *view,
                                unsigned hour,unsigned minute,
                                unsigned day,unsigned month,unsigned year){
    if(!view||!view->active)return -1;
    if(hour>23||minute>59||day<1||day>31||month<1||month>12||year>9999)return -2;
    const char t[6]={
        (char)('0'+hour/10),(char)('0'+hour%10),':',
        (char)('0'+minute/10),(char)('0'+minute%10),0};
    const char d[11]={
        (char)('0'+day/10),(char)('0'+day%10),'.',
        (char)('0'+month/10),(char)('0'+month%10),'.',
        (char)('0'+year/1000),(char)('0'+year/100%10),
        (char)('0'+year/10%10),(char)('0'+year%10),0};
    lv_label_set_text(view->time_label,t);
    lv_label_set_text(view->date_label,d);
    lv_obj_align(view->time_label,LV_ALIGN_CENTER,0,-12);
    lv_obj_align(view->date_label,LV_ALIGN_CENTER,0,42);
    return 0;
}
static void monolith_lvgl8_clock_tick(lv_timer_t *timer){
    monolith_lvgl8_view *view=(monolith_lvgl8_view*)timer->user_data;
    if(!view || !view->active || !view->time_source)return;
    unsigned h=0,m=0,d=0,mo=0,y=0;
    if(view->time_source(view->time_context,&h,&m,&d,&mo,&y)==0)
        (void)monolith_lvgl8_set_datetime(view,h,m,d,mo,y);
}
int monolith_lvgl8_auto_clock(monolith_lvgl8_view *view,
                              monolith_time_source source,void *context){
    if(!view || !view->active || !source || view->timer)return -1;
    view->time_source=source;view->time_context=context;
    view->timer=lv_timer_create(monolith_lvgl8_clock_tick,1000,view);
    if(!view->timer){view->time_source=NULL;view->time_context=NULL;return -2;}
    monolith_lvgl8_clock_tick(view->timer);
    return 0;
}
int monolith_lvgl8_pause(monolith_lvgl8_view *view){
    if(!view||!view->active)return -1;
    lv_obj_add_flag(view->image,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->time_label,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->date_label,LV_OBJ_FLAG_HIDDEN);
    if(view->timer)lv_timer_pause(view->timer);
    return 0;
}
int monolith_lvgl8_resume(monolith_lvgl8_view *view){
    if(!view||!view->active)return -1;
    lv_obj_clear_flag(view->image,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(view->time_label,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(view->date_label,LV_OBJ_FLAG_HIDDEN);
    if(view->timer)lv_timer_resume(view->timer);
    return 0;
}
void monolith_lvgl8_destroy(monolith_lvgl8_view *view){
    if(!view)return;
    if(view->timer)lv_timer_del(view->timer);
    if(view->date_label)lv_obj_del(view->date_label);
    if(view->time_label)lv_obj_del(view->time_label);
    if(view->image)lv_obj_del(view->image);
    free(view->owned_pixels);
    memset(view,0,sizeof(*view));
}
