#include "monolith_renderer.h"
#if defined(MONOLITH_FREESTANDING_ARM)
extern void *malloc(size_t);
extern void free(void *);
extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
#else
#include <stdlib.h>
#include <string.h>
#endif
enum { FRAME_BYTES = MONOLITH_WIDTH * MONOLITH_HEIGHT * MONOLITH_CHANNELS };
static const unsigned char digit[10][7] = {
{0x0e,0x11,0x13,0x15,0x19,0x11,0x0e},
{0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
{0x0e,0x11,0x01,0x02,0x04,0x08,0x1f},
{0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
{0x02,0x06,0x0a,0x12,0x1f,0x02,0x02},
{0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
{0x06,0x08,0x10,0x1e,0x11,0x11,0x0e},
{0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
{0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},
{0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c}
};
static void paint(monolith_renderer *r,int x,int y,int scale,int number){
    if(number<0 || number>9)return;
    for(int row=0;row<7;row++)
        for(int col=0;col<5;col++)
            if(digit[number][row] & (1u << (4-col)))
                for(int yy=0; yy<scale;yy++)
                    for(int xx=0;xx<scale;xx++){
                        int px=x+col*scale+xx,py=y+row*scale+yy;
                        if(px>=0 && px<MONOLITH_WIDTH && py>=0 && py<MONOLITH_HEIGHT){
                            size_t i=3u*((size_t)py*MONOLITH_WIDTH+(size_t)px);
                            r->framebuffer[i]=226;r->framebuffer[i+1]=234;r->framebuffer[i+2]=239;
                        }
                    }
}
int monolith_renderer_init(monolith_renderer *r,const uint8_t *rgb,size_t n){
    if(!r||!rgb||n!=FRAME_BYTES)return MONOLITH_BAD_ARGUMENT;
    if(r->initialized)return MONOLITH_BAD_STATE;
    uint8_t *p=(uint8_t*)malloc(FRAME_BYTES);
    if(!p)return MONOLITH_NO_MEMORY;
    memset(r,0,sizeof(*r));
    r->framebuffer=p;r->background=rgb;r->capacity=FRAME_BYTES;
    r->initialized=1;r->owns_memory=1;
    return MONOLITH_OK;
}
int monolith_image_create(monolith_renderer *r,const uint8_t *rgb,size_t n){
    if(!r || !r->initialized || !rgb || n!=FRAME_BYTES)return MONOLITH_BAD_ARGUMENT;
    r->background=rgb;return MONOLITH_OK;
}
int monolith_renderer_draw(monolith_renderer *r,unsigned h,unsigned m){
    if(!r||!r->initialized||r->paused)return MONOLITH_BAD_STATE;
    if(h>23||m>59)return MONOLITH_BAD_ARGUMENT;
    memcpy(r->framebuffer,r->background,FRAME_BYTES);
    const int s=10,y=224;const int x=112;
    paint(r,x,y,s,(int)(h/10));paint(r,x+65,y,s,(int)(h%10));
    paint(r,x+155,y,s,(int)(m/10));paint(r,x+220,y,s,(int)(m%10));
    /* Light metallic colon, not a replacement for original background. */
    for(int dy=0;dy<2;dy++)
       for(int dx=0;dx<2;dx++){
           int cy=y+22+dy*24,cx=x+135+dx*2;
           if(cy<MONOLITH_HEIGHT && cx<MONOLITH_WIDTH){
               size_t idx=3u*((size_t)cy*MONOLITH_WIDTH+(size_t)cx);
               r->framebuffer[idx]=225;r->framebuffer[idx+1]=230;r->framebuffer[idx+2]=238;
           }
       }
    r->draw_count++;
    return MONOLITH_OK;
}
int monolith_renderer_pause(monolith_renderer *r){
    if(!r||!r->initialized)return MONOLITH_BAD_STATE;
    r->paused=1;return MONOLITH_OK;
}
int monolith_renderer_resume(monolith_renderer *r){
    if(!r||!r->initialized)return MONOLITH_BAD_STATE;
    r->paused=0;return MONOLITH_OK;
}
void monolith_renderer_deinit(monolith_renderer *r){
    if(!r)return;
    if(r->owns_memory)free(r->framebuffer);
    memset(r,0,sizeof(*r));
}
const uint8_t *monolith_renderer_frame(const monolith_renderer *r){
    return (r&&r->initialized)?r->framebuffer:NULL;
}
size_t monolith_renderer_allocated(const monolith_renderer *r){
    return (r&&r->initialized&&r->owns_memory)?r->capacity:0;
}
