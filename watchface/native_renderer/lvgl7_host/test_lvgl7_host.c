#include "monolith_lvgl7_adapter.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { W=485,H=520,FRAME=W*H*3 };
static uint8_t frame[FRAME];
static lv_color_t drawing[W*20];
static lv_disp_buf_t display_buffer;
static lv_disp_drv_t display_driver;
static unsigned flushes,calls;
static int source(void *ctx,unsigned *h,unsigned *m,unsigned *d,unsigned *mo,unsigned *y){
    (void)ctx;*h=10;*m=46u+(calls%4u);*d=24;*mo=10;*y=2026;calls++;
    return 0;
}
static void flush(lv_disp_drv_t *drv,const lv_area_t *a,lv_color_t *colors){
    for(int y=a->y1;y<=a->y2;y++){
      if(y<0||y>=H)continue;
      for(int x=a->x1;x<=a->x2;x++){
        if(x<0||x>=W)continue;
        lv_color_t v=colors[(size_t)(y-a->y1)*(size_t)(a->x2-a->x1+1)+(size_t)(x-a->x1)];
        size_t at=3u*((size_t)y*W+(size_t)x);
        frame[at]=v.ch.red;frame[at+1]=v.ch.green;frame[at+2]=v.ch.blue;
      }
    }
    flushes++;
    lv_disp_flush_ready(drv);
}
int main(int argc,char **argv){
    if(argc!=3)return 1;
    FILE *in=fopen(argv[1],"rb");if(!in)return 2;
    uint8_t *rgb=malloc(FRAME);if(!rgb)return 3;
    if(fread(rgb,1,FRAME,in)!=FRAME)return 4;
    fclose(in);
    lv_init();
    lv_disp_buf_init(&display_buffer,drawing,NULL,W*20);
    lv_disp_drv_init(&display_driver);
    display_driver.hor_res=W;display_driver.ver_res=H;
    display_driver.buffer=&display_buffer;display_driver.flush_cb=flush;
    if(!lv_disp_drv_register(&display_driver))return 5;
    monolith_lvgl7_view view={0};
    if(monolith_lvgl7_create(&view,lv_scr_act(),rgb,FRAME))return 6;
    if(monolith_lvgl7_auto_clock(&view,source,NULL))return 7;
    if(calls!=1)return 8;
    for(int i=0;i<12;i++){lv_tick_inc(30);lv_task_handler();}
    if(flushes==0)return 9;
    uint8_t before[FRAME];memcpy(before,frame,FRAME);
    if(monolith_lvgl7_pause(&view))return 10;
    for(int i=0;i<3;i++){lv_tick_inc(30);lv_task_handler();}
    if(monolith_lvgl7_resume(&view))return 11;
    lv_tick_inc(1100);
    for(int i=0;i<10;i++){lv_tick_inc(30);lv_task_handler();}
    if(calls<2||!memcmp(before,frame,FRAME))return 12;
    FILE *out=fopen(argv[2],"wb");if(!out)return 13;
    fprintf(out,"P6\n485 520\n255\n");
    if(fwrite(frame,1,FRAME,out)!=FRAME)return 14;
    fclose(out);
    monolith_lvgl7_destroy(&view);
    lv_mem_monitor_t mem_before={0},mem_after={0};
    lv_mem_monitor(&mem_before);
    for(unsigned i=0;i<1000;i++){
      monolith_lvgl7_view v={0};
      if(monolith_lvgl7_create(&v,lv_scr_act(),rgb,FRAME))return 15;
      if(monolith_lvgl7_auto_clock(&v,source,NULL))return 16;
      if(monolith_lvgl7_pause(&v)||monolith_lvgl7_resume(&v))return 17;
      monolith_lvgl7_destroy(&v);
      if(v.active||v.task||v.owned_pixels)return 18;
    }
    for(int i=0;i<5;i++){lv_tick_inc(30);lv_task_handler();}
    lv_mem_monitor(&mem_after);
    if(mem_after.free_size+1024u<mem_before.free_size)return 19;
    free(rgb);
    printf("PASS: LVGL7 real framebuffer time/date, 1000 lifecycles; heap %u->%u; flushes %u\n",
           (unsigned)mem_before.free_size,(unsigned)mem_after.free_size,flushes);
    return 0;
}
