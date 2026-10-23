/* Headless LVGL v8 pixel flush integration.
 * Links against the actual public LVGL 8.3.11 library source.
 * No OEM/RT-Thread module load, BLE or hardware access.
 */
#include "monolith_lvgl_v8_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIDTH 485
#define HEIGHT 520
static uint8_t *framebuffer;
static size_t flushes;
static void flush_frame(lv_disp_drv_t *driver,const lv_area_t *rect,lv_color_t *data){
    for(int y=rect->y1;y<=rect->y2;y++){
        for(int x=rect->x1;x<=rect->x2;x++){
            const lv_color_t color=*data++;
            if(x<0||x>=WIDTH||y<0||y>=HEIGHT)continue;
            size_t k=((size_t)y*WIDTH+x)*3u;
            framebuffer[k]=color.ch.red;
            framebuffer[k+1]=color.ch.green;
            framebuffer[k+2]=color.ch.blue;
        }
    }
    flushes++;
    lv_disp_flush_ready(driver);
}
static uint8_t *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);
    if(n<=0||n>MONOLITH_RESOURCE_MAX_BYTES||fseek(f,0,SEEK_SET)){
        fclose(f);return NULL;
    }
    uint8_t *data=malloc((size_t)n);
    if(!data){fclose(f);return NULL;}
    *size=fread(data,1,(size_t)n,f);
    fclose(f);
    if(*size!=(size_t)n){free(data);return NULL;}
    return data;
}
int main(int argc,char **argv){
    if(argc!=3)return 2;
    size_t n=0;uint8_t *file=read_file(argv[1],&n);
    if(!file)return 3;
    monolith_resource res={0};
    int rc=monolith_resource_open(&res,file,n);
    if(!rc)rc=monolith_resource_decode(&res);
    if(rc){fprintf(stderr,"decode error %d\n",rc);free(file);return 4;}
    framebuffer=calloc((size_t)WIDTH*HEIGHT*3u,1);
    lv_color_t *drawbuf=calloc((size_t)WIDTH*20u,sizeof(lv_color_t));
    if(!framebuffer||!drawbuf)return 5;
    lv_init();
    lv_disp_draw_buf_t dbuf;
    lv_disp_draw_buf_init(&dbuf,drawbuf,NULL,WIDTH*20u);
    lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res=WIDTH;driver.ver_res=HEIGHT;
    driver.draw_buf=&dbuf;driver.flush_cb=flush_frame;
    if(!lv_disp_drv_register(&driver))return 6;
    monolith_lvgl_v8 view={0};
    if(monolith_lvgl_v8_open(&view,lv_scr_act(),&res))return 7;
    if(monolith_lvgl_v8_update(&view,12,34,23,10,2026))return 8;
    if(monolith_lvgl_v8_update(&view,12,34,23,10,2026)||view.updates!=1)return 9;
    if(monolith_lvgl_v8_pause(&view))return 10;
    if(monolith_lvgl_v8_update(&view,12,35,23,10,2026)||view.updates!=1)return 11;
    if(monolith_lvgl_v8_resume(&view))return 12;
    if(monolith_lvgl_v8_update(&view,12,34,23,10,2026)||view.updates!=2)return 13;
    lv_tick_inc(100);
    for(int i=0;i<6;i++)lv_timer_handler();
    if(flushes==0){fprintf(stderr,"no LVGL flushes\n");return 14;}
    FILE *out=fopen(argv[2],"wb");
    if(!out)return 15;
    fprintf(out,"P6\n%d %d\n255\n",WIDTH,HEIGHT);
    const size_t total=(size_t)WIDTH*HEIGHT*3u;
    if(fwrite(framebuffer,1,total,out)!=total){fclose(out);return 16;}
    fclose(out);
    size_t nonzero=0;
    for(size_t k=0;k<total;k++)nonzero+=framebuffer[k]!=0;
    if(nonzero<total/20u){fprintf(stderr,"LVGL frame is too empty\n");return 17;}
    printf("PASS: real LVGL v8 frame, %zu flushes, %zu/%zu nonzero bytes, updates=%u\n",
           flushes,nonzero,total,view.updates);
    monolith_lvgl_v8_close(&view);
    monolith_lvgl_v8_close(&view);
    monolith_resource_release(&res);
    free(file);
    lv_deinit();
    free(drawbuf);free(framebuffer);
    return 0;
}
