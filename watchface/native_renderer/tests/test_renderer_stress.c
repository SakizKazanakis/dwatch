/* Real-source C renderer test: 1000 create/pause/resume/close cycles. */
#include "monolith_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *load(const char *name,size_t *n) {
    FILE *f=fopen(name,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}
    long size=ftell(f);
    if(size<=0||size>MONOLITH_RESOURCE_MAX_BYTES||fseek(f,0,SEEK_SET)){
        fclose(f);return 0;
    }
    uint8_t *p=malloc((size_t)size);
    if(!p){fclose(f);return 0;}
    *n=fread(p,1,(size_t)size,f);fclose(f);
    if(*n!=(size_t)size){free(p);return 0;}
    return p;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    size_t n=0;uint8_t *bytes=load(argv[1],&n);
    if(!bytes)return 2;
    monolith_resource r={0};
    int rc=monolith_resource_open(&r,bytes,n);
    if(!rc)rc=monolith_resource_decode(&r);
    if(rc||r.rgb_size!=756600u){free(bytes);return 3;}
    for(unsigned i=0;i<1000;i++){
        monolith_renderer view={0};
        if(monolith_renderer_init(&view,&r))return 4;
        if(memcmp(view.canvas,r.rgb,r.rgb_size))return 5;
        if(monolith_renderer_draw(&view,12,34,23,10,2026))return 6;
        if(view.updates!=1||!memcmp(view.canvas,r.rgb,r.rgb_size))return 7;
        if(monolith_renderer_draw(&view,12,34,23,10,2026)||view.updates!=1)return 8;
        if(monolith_renderer_pause(&view))return 9;
        if(monolith_renderer_draw(&view,12,35,23,10,2026)||view.updates!=1)return 10;
        if(monolith_renderer_resume(&view))return 11;
        if(monolith_renderer_draw(&view,12,35,23,10,2026)||view.updates!=2)return 12;
        if(monolith_renderer_draw(&view,25,0,23,10,2026)==0)return 13;
        if(monolith_renderer_draw(&view,12,35,31,2,2026)==0)return 14;
        monolith_renderer_deinit(&view);monolith_renderer_deinit(&view);
        if(view.canvas||view.image||view.active)return 15;
    }
    monolith_resource_release(&r);monolith_resource_release(&r);
    free(bytes);
    puts("PASS: 1000 native lifecycle cycles; unchanged minutes do not redraw");
    return 0;
}
