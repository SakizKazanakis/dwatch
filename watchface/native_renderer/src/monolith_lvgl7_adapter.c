#include "monolith_lvgl7_adapter.h"
#include "monolith_renderer.h"
#include <stdlib.h>
#include <string.h>
#define MW 485u
#define MH 520u
static int monolith_lvgl7_finish(monolith_lvgl7_view *v,lv_obj_t *root);
int monolith_lvgl7_create(monolith_lvgl7_view *v,lv_obj_t *root,
                          const uint8_t *rgb,size_t bytes){
    if(!v||!root||!rgb||bytes!=MW*MH*3u||v->active)return -1;
    memset(v,0,sizeof(*v));
    v->owned_pixels=(lv_color_t*)malloc(MW*MH*sizeof(lv_color_t));
    if(!v->owned_pixels)return -2;
    for(size_t i=0;i<MW*MH;i++)
        v->owned_pixels[i]=lv_color_make(rgb[3*i],rgb[3*i+1],rgb[3*i+2]);
    return monolith_lvgl7_finish(v,root);
}
static int monolith_lvgl7_finish(monolith_lvgl7_view *v,lv_obj_t *root){
    v->descriptor.header.always_zero=0;
    v->descriptor.header.w=MW;
    v->descriptor.header.h=MH;
    v->descriptor.header.cf=LV_IMG_CF_TRUE_COLOR;
    v->descriptor.data_size=MW*MH*sizeof(lv_color_t);
    v->descriptor.data=(const uint8_t*)v->owned_pixels;
    v->image=lv_img_create(root,NULL);
    if(!v->image){monolith_lvgl7_destroy(v);return -3;}
    lv_img_set_src(v->image,&v->descriptor);
    lv_obj_align(v->image,NULL,LV_ALIGN_CENTER,0,0);
    v->time_label=lv_label_create(root,NULL);
    v->date_label=lv_label_create(root,NULL);
    if(!v->time_label||!v->date_label){monolith_lvgl7_destroy(v);return -4;}
    lv_obj_set_style_local_text_color(v->time_label,LV_LABEL_PART_MAIN,LV_STATE_DEFAULT,lv_color_hex(0xE5EFF5));
    lv_obj_set_style_local_text_color(v->date_label,LV_LABEL_PART_MAIN,LV_STATE_DEFAULT,lv_color_hex(0x9EB4C2));
    lv_obj_align(v->time_label,NULL,LV_ALIGN_CENTER,0,-12);
    lv_obj_align(v->date_label,NULL,LV_ALIGN_CENTER,0,42);
    v->active=1;
    return 0;
}
typedef struct monolith_lvgl7_stream {
    lv_color_t *pixels;
    unsigned rows;
} monolith_lvgl7_stream;
static int monolith_lvgl7_scanline(void *opaque,unsigned row,
                                  const uint8_t *rgb,size_t bytes){
    monolith_lvgl7_stream *state=(monolith_lvgl7_stream*)opaque;
    if(!state||row!=state->rows||bytes!=MW*3u||row>=MH)return -1;
    lv_color_t *dst=state->pixels+(size_t)row*MW;
    for(unsigned x=0;x<MW;x++)
        dst[x]=lv_color_make(rgb[3u*x],rgb[3u*x+1u],rgb[3u*x+2u]);
    state->rows++;
    return 0;
}
int monolith_lvgl7_create_from_ezip(monolith_lvgl7_view *v,lv_obj_t *root,
                                    const uint8_t *ezip,size_t bytes){
    if(!v||!root||!ezip||v->active)return -1;
    memset(v,0,sizeof(*v));
    lv_color_t *pixels=(lv_color_t*)malloc(MW*MH*sizeof(lv_color_t));
    if(!pixels)return -2;
    monolith_lvgl7_stream stream={pixels,0};
    if(monolith_resource_decode_scanlines(ezip,bytes,monolith_lvgl7_scanline,&stream)!=MONOLITH_OK
        || stream.rows!=MH){
        free(pixels);
        return -3;
    }
    v->owned_pixels=pixels;
    return monolith_lvgl7_finish(v,root);
}
int monolith_lvgl7_set_datetime(monolith_lvgl7_view *v,
                                unsigned h,unsigned m,unsigned d,unsigned mo,unsigned y){
    if(!v||!v->active)return -1;
    if(h>23||m>59||d<1||d>31||mo<1||mo>12||y>9999)return -2;
    const char time[6]={(char)('0'+h/10),(char)('0'+h%10),':',
        (char)('0'+m/10),(char)('0'+m%10),0};
    const char date[11]={(char)('0'+d/10),(char)('0'+d%10),'.',
        (char)('0'+mo/10),(char)('0'+mo%10),'.',
        (char)('0'+y/1000),(char)('0'+y/100%10),
        (char)('0'+y/10%10),(char)('0'+y%10),0};
    lv_label_set_text(v->time_label,time);
    lv_label_set_text(v->date_label,date);
    lv_obj_align(v->time_label,NULL,LV_ALIGN_CENTER,0,-12);
    lv_obj_align(v->date_label,NULL,LV_ALIGN_CENTER,0,42);
    return 0;
}
static void monolith_lvgl7_tick(lv_task_t *task){
    monolith_lvgl7_view *v=(monolith_lvgl7_view*)task->user_data;
    if(!v||!v->active||!v->source)return;
    unsigned h=0,m=0,d=0,mo=0,y=0;
    if(v->source(v->source_ctx,&h,&m,&d,&mo,&y)==0)
        (void)monolith_lvgl7_set_datetime(v,h,m,d,mo,y);
}
int monolith_lvgl7_auto_clock(monolith_lvgl7_view *v,
                              monolith_lvgl7_time_source source,void *ctx){
    if(!v||!v->active||!source||v->task)return -1;
    v->source=source;v->source_ctx=ctx;
    v->task=lv_task_create(monolith_lvgl7_tick,1000,LV_TASK_PRIO_MID,v);
    if(!v->task){v->source=NULL;v->source_ctx=NULL;return -2;}
    monolith_lvgl7_tick(v->task);
    return 0;
}
int monolith_lvgl7_pause(monolith_lvgl7_view *v){
    if(!v||!v->active)return -1;
    lv_obj_set_hidden(v->image,true);
    lv_obj_set_hidden(v->time_label,true);
    lv_obj_set_hidden(v->date_label,true);
    if(v->task)lv_task_set_prio(v->task,LV_TASK_PRIO_OFF);
    return 0;
}
int monolith_lvgl7_resume(monolith_lvgl7_view *v){
    if(!v||!v->active)return -1;
    lv_obj_set_hidden(v->image,false);
    lv_obj_set_hidden(v->time_label,false);
    lv_obj_set_hidden(v->date_label,false);
    if(v->task)lv_task_set_prio(v->task,LV_TASK_PRIO_MID);
    return 0;
}
void monolith_lvgl7_destroy(monolith_lvgl7_view *v){
    if(!v)return;
    if(v->task)lv_task_del(v->task);
    if(v->date_label)lv_obj_del(v->date_label);
    if(v->time_label)lv_obj_del(v->time_label);
    if(v->image)lv_obj_del(v->image);
    free(v->owned_pixels);
    memset(v,0,sizeof(*v));
}
