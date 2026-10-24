#include "monolith_lvgl8_adapter.h"
#include "monolith_renderer.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W=485,H=520,FRAME=W*H*3 };
static unsigned char frame[FRAME];
static lv_color_t drawbuffer[W*20];
static lv_disp_draw_buf_t drawbuf;
static lv_disp_drv_t driver;
static unsigned flushes;
static void flush(lv_disp_drv_t *drv,const lv_area_t *a,lv_color_t *pixels){
    for(int y=a->y1;y<=a->y2;y++){
        if(y<0||y>=H)continue;
        for(int x=a->x1;x<=a->x2;x++){
            if(x<0||x>=W)continue;
            lv_color_t v=pixels[(size_t)(y-a->y1)*(size_t)(a->x2-a->x1+1)+(size_t)(x-a->x1)];
            size_t idx=3u*((size_t)y*W+(size_t)x);
            frame[idx]=v.ch.red;frame[idx+1]=v.ch.green;frame[idx+2]=v.ch.blue;
        }
    }
    flushes++;
    lv_disp_flush_ready(drv);
}
static unsigned provider_calls;
static int fake_clock(void *ctx,unsigned *h,unsigned *m,
                      unsigned *day,unsigned *month,unsigned *year){
    (void)ctx;
    *h=10;*m=46u+(provider_calls%4u);
    *day=24;*month=10;*year=2026;
    provider_calls++;
    return 0;
}
int main(int argc,char **argv){
    if(argc!=3)return 1;
    FILE *in=fopen(argv[1],"rb");if(!in)return 2;
    unsigned char *rgb=malloc(FRAME);if(!rgb)return 3;
    if(fread(rgb,1,FRAME,in)!=FRAME)return 4;
    fclose(in);
    lv_init();
    lv_disp_draw_buf_init(&drawbuf,drawbuffer,NULL,W*20);
    lv_disp_drv_init(&driver);
    driver.hor_res=W;driver.ver_res=H;
    driver.flush_cb=flush;driver.draw_buf=&drawbuf;
    if(!lv_disp_drv_register(&driver))return 5;
    monolith_lvgl8_view view={0};
    if(monolith_lvgl8_create(&view,lv_scr_act(),rgb,FRAME))return 6;
    if(monolith_lvgl8_auto_clock(&view,fake_clock,NULL))return 7;
    if(provider_calls!=1)return 19;
    for(int i=0;i<12;i++){lv_tick_inc(30);lv_timer_handler();}
    if(flushes==0)return 8;
    unsigned char before[FRAME];
    memcpy(before,frame,FRAME);
    if(monolith_lvgl8_pause(&view))return 9;
    for(int i=0;i<4;i++){lv_tick_inc(30);lv_timer_handler();}
    if(monolith_lvgl8_resume(&view))return 10;
    lv_tick_inc(1100);
    for(int i=0;i<4;i++){lv_tick_inc(30);lv_timer_handler();}
    if(provider_calls<2)return 11;
    for(int i=0;i<10;i++){lv_tick_inc(30);lv_timer_handler();}
    if(!memcmp(before,frame,FRAME))return 12;
    FILE *out=fopen(argv[2],"wb");if(!out)return 13;
    fprintf(out,"P6\n485 520\n255\n");
    if(fwrite(frame,1,FRAME,out)!=FRAME)return 14;
    fclose(out);
    monolith_lvgl8_destroy(&view);
    monolith_lvgl8_destroy(&view);
    for(int i=0;i<4;i++){lv_tick_inc(30);lv_timer_handler();}
    lv_mem_monitor_t mem_before={0},mem_after={0};
    lv_mem_monitor(&mem_before);
    for(unsigned i=0;i<1000;i++){
        monolith_lvgl8_view repeat={0};
        if(monolith_lvgl8_create(&repeat,lv_scr_act(),rgb,FRAME))return 20;
        if(monolith_lvgl8_auto_clock(&repeat,fake_clock,NULL))return 21;
        if(monolith_lvgl8_pause(&repeat)||monolith_lvgl8_resume(&repeat))return 22;
        monolith_lvgl8_destroy(&repeat);
        if(repeat.active || repeat.owned_pixels || repeat.timer)return 23;
    }
    for(int i=0;i<6;i++){lv_tick_inc(30);lv_timer_handler();}
    lv_mem_monitor(&mem_after);
    if(mem_after.free_size+1024u<mem_before.free_size){
        fprintf(stderr,"LVGL heap leak: free before=%u after=%u\n",
                (unsigned)mem_before.free_size,(unsigned)mem_after.free_size);
        return 24;
    }
    free(rgb);
    printf("PASS: public LVGL8 display, timer, 1000 lifecycle iterations, LVGL free bytes %u->%u; %u flushes\n",
           (unsigned)mem_before.free_size,(unsigned)mem_after.free_size,flushes);
    return 0;
}
