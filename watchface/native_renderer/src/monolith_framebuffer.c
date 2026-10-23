/* MONOLITH native watchface framebuffer compositor.
 * Uses ONLY pixels independently decoded from clock_bg.bin; never loads master
 * PNG. Host framebuffer implementation, not a proven HK8 hardware renderer.
 */
#include "monolith_renderer.h"
#include <stdlib.h>
#include <string.h>

typedef struct {char chr;uint8_t rows[7];} Glyph;
static const Glyph GLYPHS[]={
 {'0',{14,17,19,21,25,17,14}},
 {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}},
 {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}},
 {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}},
 {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}},
 {'9',{14,17,17,15,1,1,14}},
 {':',{0,4,4,0,4,4,0}},
 {'.',{0,0,0,0,0,12,12}}
};

static const uint8_t *glyph(char c){
    for(unsigned i=0;i<sizeof(GLYPHS)/sizeof(GLYPHS[0]);i++)
        if(GLYPHS[i].chr==c)return GLYPHS[i].rows;
    return 0;
}
static void put(monolith_renderer *ctx,int x,int y,unsigned r,unsigned g,unsigned b){
    if(x<0||y<0||(unsigned)x>=ctx->width||(unsigned)y>=ctx->height)return;
    size_t p=((size_t)y*ctx->width+(unsigned)x)*3u;
    ctx->canvas[p]=(uint8_t)r;
    ctx->canvas[p+1]=(uint8_t)g;
    ctx->canvas[p+2]=(uint8_t)b;
}
static void draw_text(monolith_renderer *ctx,const char *txt,int cx,int cy,
                      int s,unsigned r,unsigned g,unsigned b){
    int n=0;while(txt[n])n++;
    const int width=n*(6*s)-s;
    const int left=cx-width/2;
    const int top=cy-(7*s)/2;
    for(int i=0;i<n;i++){
        const uint8_t *ch=glyph(txt[i]);
        if(!ch)continue;
        for(int y=0;y<7;y++)for(int x=0;x<5;x++)if((ch[y]>>(4-x))&1u){
            for(int dy=0;dy<s;dy++)for(int dx=0;dx<s;dx++)
                put(ctx,left+(i*6+x)*s+dx,top+y*s+dy,r,g,b);
        }
    }
}
static int valid_date(unsigned d,unsigned m,unsigned y) {
    if(y<1970||y>2099||m<1||m>12||d<1)return 0;
    static const unsigned days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned count=days[m];
    if(m==2&&((y%4==0&&y%100!=0)||y%400==0))count++;
    return d<=count;
}
static void decimal2(char *out,unsigned n) {
    out[0]=(char)('0'+n/10);out[1]=(char)('0'+n%10);
}
int monolith_renderer_init(monolith_renderer *ctx,monolith_resource *res) {
    if(!ctx||!res||!res->rgb||!res->width||!res->height)return MONOLITH_INVALID;
    memset(ctx,0,sizeof(*ctx));
    ctx->canvas=malloc(res->rgb_size);
    if(!ctx->canvas)return MONOLITH_NOMEM;
    ctx->image=res;
    ctx->canvas_size=res->rgb_size;
    ctx->width=res->width;ctx->height=res->height;
    memcpy(ctx->canvas,res->rgb,res->rgb_size);
    ctx->active=1;
    ctx->last_minute=~0u;ctx->last_date=~0u;
    return MONOLITH_OK;
}
int monolith_renderer_draw(monolith_renderer *ctx,unsigned hour,unsigned minute,
                           unsigned day,unsigned month,unsigned year) {
    if(!ctx||!ctx->image||!ctx->canvas)return MONOLITH_STATE;
    if(!ctx->active)return MONOLITH_OK; /* paused; do not touch framebuffer */
    if(hour>23||minute>59||!valid_date(day,month,year))return MONOLITH_INVALID;
    unsigned mk=hour*60u+minute;
    unsigned dk=year*512u+month*32u+day;
    if(mk==ctx->last_minute&&dk==ctx->last_date)return MONOLITH_OK;
    /* Repaint exactly from the verified eZIP pixel buffer. Decorations,
     * DKRNN logo, borders and typography from the master image stay intact. */
    memcpy(ctx->canvas,ctx->image->rgb,ctx->canvas_size);
    char clock[]="00:00";
    decimal2(clock,hour);decimal2(clock+3,minute);
    char date[]="00.00.0000";
    decimal2(date,day);decimal2(date+3,month);
    date[6]=(char)('0'+(year/1000u)%10u);
    date[7]=(char)('0'+(year/100u)%10u);
    date[8]=(char)('0'+(year/10u)%10u);
    date[9]=(char)('0'+year%10u);
    /* Chosen overlay region in original 485x520 design, not verified OEM
     * watchface geometry/paint style. */
    draw_text(ctx,clock,(int)ctx->width/2,(int)(ctx->height*49u/100u),
              10u,232u,239u,242u);
    draw_text(ctx,date,(int)ctx->width/2,(int)(ctx->height*58u/100u),
              2u,145u,165u,178u);
    ctx->last_minute=mk;ctx->last_date=dk;
    ctx->updates++;
    return MONOLITH_OK;
}
int monolith_renderer_pause(monolith_renderer *ctx){
    if(!ctx||!ctx->canvas)return MONOLITH_STATE;
    ctx->active=0;return MONOLITH_OK;
}
int monolith_renderer_resume(monolith_renderer *ctx){
    if(!ctx||!ctx->canvas)return MONOLITH_STATE;
    ctx->active=1;ctx->last_minute=~0u;ctx->last_date=~0u;
    return MONOLITH_OK;
}
void monolith_renderer_deinit(monolith_renderer *ctx){
    if(!ctx)return;
    free(ctx->canvas);memset(ctx,0,sizeof(*ctx));
}
