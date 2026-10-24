/* Monolith 485x520 SiFli legacy shared-Huffman eZIP decoder.
 * Independent C implementation of the binary structure validated against
 * the actual 158937-byte original. Untrusted inputs fail closed.
 *
 * No ROM, LVGL or device hardware APIs are used; this decoder is host/ARM
 * compatible but requires ~757120 bytes temporary RAM in its current form.
 */
#include "monolith_renderer.h"
#include <stddef.h>
#include <stdint.h>
#ifdef MONOLITH_FREESTANDING_ARM
extern void *malloc(size_t);
extern void free(void *);
#else
#include <stdlib.h>
#endif

#define OUT_BYTES (MONOLITH_HEIGHT * (MONOLITH_WIDTH*3+1))
#define PIXEL_BYTES (MONOLITH_HEIGHT * MONOLITH_WIDTH * 3)
#define STREAM_OFFSET (4u+88u)
#define MAX_CODES 320
/* CRC32/IEEE of the user's exact original clock_bg.bin, verified offline
 * against SHA256. This decoder is intentionally source-specific. */
static int monolith_original_crc_ok(const uint8_t *input,size_t n){
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<n;i++){
        crc^=input[i];
        for(unsigned k=0;k<8u;k++)
            crc=(crc&1u)?(crc>>1)^0xedb88320u:(crc>>1);
    }
    return (~crc)==0x8735d151u;
}


static const uint16_t lens[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t lextra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t dists[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t dextra[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const uint8_t order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

typedef struct bits {
    const uint8_t *data;size_t len;size_t pos;int error;
} bits;
static unsigned get(bits *b,unsigned n){
    if(n>24 || b->pos > b->len*8 || n>b->len*8-b->pos){
        b->error=1;return 0;
    }
    unsigned v=0;
    for(unsigned i=0;i<n;i++){
        size_t p=b->pos++;
        v|=((unsigned)(b->data[p>>3]>>(p&7u))&1u)<<i;
    }
    return v;
}
static int align4(bits *b){
    size_t end=((b->pos+7u)/8u+3u)/4u*32u;
    if(end>b->len*8u){b->error=1;return -1;}
    b->pos=end;return 0;
}
typedef struct huffman {
    unsigned first[16],count[16],prefix[16];
    unsigned short symbols[MAX_CODES];
    unsigned total;
} huffman;
static int mkhuff(huffman *h,const uint8_t *length,unsigned n){
    if(n>MAX_CODES)return -1;
    for(int j=0;j<16;j++){h->count[j]=0;h->first[j]=0;h->prefix[j]=0;}
    for(unsigned i=0;i<n;i++){
        if(length[i]>15)return -1;
        if(length[i])h->count[length[i]]++;
    }
    unsigned code=0,offset=0;
    for(unsigned len=1;len<=15;len++){
        code=(code+h->count[len-1])<<1;
        if(code+h->count[len]>(1u<<len))return -1;
        h->first[len]=code;h->prefix[len]=offset;offset+=h->count[len];
    }
    h->total=offset;
    for(unsigned len=1;len<=15;len++){
        unsigned at=h->prefix[len];
        for(unsigned sym=0;sym<n;sym++)if(length[sym]==len)h->symbols[at++]=(unsigned short)sym;
    }
    return offset?0:-1;
}
static int getsym(bits *b,const huffman *h){
    unsigned code=0;
    for(unsigned n=1;n<=15;n++){
        code=(code<<1)|get(b,1);
        if(b->error)return -1;
        if(code>=h->first[n] && code-h->first[n]<h->count[n])
            return (int)h->symbols[h->prefix[n]+code-h->first[n]];
    }
    b->error=1;return -1;
}
static int shared_decode(const uint8_t *source,size_t len,uint8_t *out){
    bits b={source,len,0,0};
    unsigned nlit=get(&b,5)+257,ndist=get(&b,5)+1,norder=get(&b,4)+4;
    if(nlit>286 || ndist>32 || b.error)return -1;
    uint8_t clen[19]={0},all[MAX_CODES]={0};
    for(unsigned i=0;i<norder;i++)clen[order[i]]=(uint8_t)get(&b,3);
    huffman ct,lit,dist;
    if(b.error||mkhuff(&ct,clen,19))return -1;
    unsigned used=0;
    while(used<nlit+ndist){
        int sym=getsym(&b,&ct);
        unsigned times=0;uint8_t value=0;
        if(sym<0)return -1;
        if(sym<16){all[used++]=(uint8_t)sym;continue;}
        if(sym==16){if(!used)return -1;times=get(&b,2)+3;value=all[used-1];}
        else if(sym==17)times=get(&b,3)+3;
        else if(sym==18)times=get(&b,7)+11;
        else return -1;
        if(b.error||times>nlit+ndist-used)return -1;
        while(times--)all[used++]=value;
    }
    if(mkhuff(&lit,all,nlit)||mkhuff(&dist,all+nlit,ndist)||align4(&b))return -1;
    size_t produced=0;
    for(;;){
        unsigned final=get(&b,1),typ=get(&b,2);
        if(b.error)return -1;
        if(typ==0){
            b.pos=(b.pos+7u)/8u*8u;
            unsigned n=get(&b,16),inverse=get(&b,16);
            if(b.error||((n^inverse)!=65535u)||n>OUT_BYTES-produced)return -1;
            while(n--)out[produced++]=(uint8_t)get(&b,8);
            if(b.error)return -1;
        } else if(typ==1 || typ==2){
            for(;;){
                int sym=getsym(&b,&lit);
                if(sym<0)return -1;
                if(sym<256){if(produced>=OUT_BYTES)return -1;out[produced++]=(uint8_t)sym;}
                else if(sym==256)break;
                else {
                    unsigned index=(unsigned)(sym-257);
                    if(index>=29)return -1;
                    unsigned count=lens[index]+get(&b,lextra[index]);
                    int d=getsym(&b,&dist);
                    if(d<0||d>=30)return -1;
                    unsigned back=dists[d]+get(&b,dextra[d]);
                    if(b.error || back>produced || count>OUT_BYTES-produced)return -1;
                    while(count--){out[produced]=out[produced-back];produced++;}
                }
            }
        }else return -1;
        if(align4(&b))return -1;
        if(final)break;
    }
    return (produced==OUT_BYTES)?0:-1;
}
static unsigned paeth(unsigned a,unsigned b,unsigned c){
    int p=(int)a+(int)b-(int)c;
    unsigned da=(unsigned)(p>(int)a?p-(int)a:(int)a-p);
    unsigned db=(unsigned)(p>(int)b?p-(int)b:(int)b-p);
    unsigned dc=(unsigned)(p>(int)c?p-(int)c:(int)c-p);
    return da<=db&&da<=dc?a:(db<=dc?b:c);
}
int monolith_resource_decode(const uint8_t *source,size_t length,uint8_t *output,size_t out_size){
    if(!source||!output||out_size<PIXEL_BYTES || length!=158937u)return MONOLITH_BAD_ARGUMENT;
    if(!monolith_original_crc_ok(source,length))return MONOLITH_BAD_ARGUMENT;
    uint32_t image=(uint32_t)source[0]|((uint32_t)source[1]<<8)
         |((uint32_t)source[2]<<16)|((uint32_t)source[3]<<24);
    if((image&31u)!=1u || ((image>>10)&2047u)!=485u ||
        ((image>>21)&2047u)!=520u)return MONOLITH_BAD_ARGUMENT;
    /* 16-byte EZIP header and 72-byte 17-block offset directory. */
    const uint8_t *header=source+4;
    uint32_t reported=((uint32_t)header[0]<<24)|((uint32_t)header[1]<<16)|
         ((uint32_t)header[2]<<8)|header[3];
    if(reported!=length-4 || header[5]!=8 || header[6]!=32 ||
       header[8]!=1 || header[9]!=229 || header[10]!=2 || header[11]!=8 ||
       header[16]!=0 || header[17]!=32 || header[18]!=0 || header[19]!=17)
       return MONOLITH_BAD_ARGUMENT;
    uint32_t prev=0;
    for(unsigned i=0;i<17;i++){
        const uint8_t *p=header+20+i*4;
        uint32_t off=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
        if(off<=prev || off>=reported)return MONOLITH_BAD_ARGUMENT;
        prev=off;
    }
    uint8_t *tmp=(uint8_t*)malloc(OUT_BYTES);
    if(!tmp)return MONOLITH_NO_MEMORY;
    int ok=shared_decode(source+STREAM_OFFSET,length-STREAM_OFFSET,tmp);
    if(ok){free(tmp);return MONOLITH_BAD_ARGUMENT;}
    /* PNG filters reset the previous scanline at each 32-row block boundary. */
    const size_t stride=MONOLITH_WIDTH*3u;
    for(size_t y=0;y<MONOLITH_HEIGHT;y++){
        size_t at=y*(stride+1u);
        uint8_t filter=tmp[at];
        if(filter>4){free(tmp);return MONOLITH_BAD_ARGUMENT;}
        for(size_t x=0;x<stride;x++){
            unsigned a=x>=3?output[y*stride+x-3]:0;
            unsigned b=(y%32u)!=0?output[(y-1u)*stride+x]:0;
            unsigned c=(y%32u)!=0 && x>=3?output[(y-1u)*stride+x-3]:0;
            unsigned predictor=filter==0?0:filter==1?a:filter==2?b:
                 filter==3?(a+b)/2u:paeth(a,b,c);
            output[y*stride+x]=(uint8_t)(tmp[at+1u+x]+predictor);
        }
    }
    free(tmp);
    /* SiFli resource format 1 / 24-bit stores BGR; convert to RGB. */
    for(size_t i=0;i<PIXEL_BYTES;i+=3){
        uint8_t t=output[i];output[i]=output[i+2];output[i+2]=t;
    }
    return MONOLITH_OK;
}


/* Low-memory alternative: maintain only the 32 KiB DEFLATE history window.
 * It incrementally defilters bytes into the caller's output image. It still
 * requires a full RGB output buffer; true on-device tiled VRAM output remains
 * dependent on an unverified firmware graphics API.
 */
typedef struct monolith_sink {
    uint8_t *history;
    uint8_t *rgb_bgr;
    size_t produced;
    unsigned filter;
    /* Optional streamed mode, using only two 1455-byte BGR rows. */
    uint8_t *rows;
    uint8_t *rgb_row;
    monolith_scanline_cb on_row;
    void *context;
} monolith_sink;

static int monolith_sink_put(monolith_sink *s,uint8_t byte){
    if(s->produced>=OUT_BYTES)return -1;
    const size_t stride=MONOLITH_WIDTH*3u;
    size_t row=s->produced/(stride+1u);
    size_t row_index=s->produced%(stride+1u);
    if(row_index==0){
        if(byte>4)return -1;
        s->filter=byte;
    }else{
        size_t x=row_index-1u;
        uint8_t *cur=s->rows ? s->rows+(row%2u)*stride
                             : s->rgb_bgr+row*stride;
        const uint8_t *prev=s->rows
              ? s->rows+((row+1u)%2u)*stride
              : (row ? s->rgb_bgr+(row-1u)*stride : (const uint8_t*)0);
        unsigned a=x>=3u?cur[x-3u]:0;
        unsigned b=row%32u?prev[x]:0;
        unsigned c=row%32u&&x>=3u?prev[x-3u]:0;
        unsigned f=s->filter;
        unsigned predict=f==0?0:f==1?a:f==2?b:f==3?(a+b)/2u:paeth(a,b,c);
        cur[x]=(uint8_t)(byte+predict);
        if(s->on_row && x+1u==stride){
            for(size_t i=0;i<stride;i+=3u){
                s->rgb_row[i]=cur[i+2u];
                s->rgb_row[i+1u]=cur[i+1u];
                s->rgb_row[i+2u]=cur[i];
            }
            if(s->on_row(s->context,(unsigned)row,s->rgb_row,stride)!=0)
                return -1;
        }
    }
    s->history[s->produced & 32767u]=byte;
    s->produced++;
    return 0;
}

static int monolith_shared_stream(const uint8_t *src,size_t len,monolith_sink *out){
    bits b={src,len,0,0};
    unsigned nlit=get(&b,5)+257,ndist=get(&b,5)+1,norder=get(&b,4)+4;
    if(nlit>286||ndist>32||b.error)return -1;
    uint8_t clen[19]={0},all[MAX_CODES]={0};
    for(unsigned i=0;i<norder;i++)clen[order[i]]=(uint8_t)get(&b,3);
    huffman ct,lit,dist;
    if(b.error||mkhuff(&ct,clen,19))return -1;
    unsigned used=0;
    while(used<nlit+ndist){
        int sym=getsym(&b,&ct);
        unsigned count=0;uint8_t value=0;
        if(sym<0)return -1;
        if(sym<16){all[used++]=(uint8_t)sym;continue;}
        if(sym==16){if(!used)return -1;count=get(&b,2)+3;value=all[used-1];}
        else if(sym==17)count=get(&b,3)+3;
        else if(sym==18)count=get(&b,7)+11;
        else return -1;
        if(b.error||count>nlit+ndist-used)return -1;
        while(count--)all[used++]=value;
    }
    if(mkhuff(&lit,all,nlit)||mkhuff(&dist,all+nlit,ndist)||align4(&b))return -1;
    for(;;){
        unsigned final=get(&b,1),kind=get(&b,2);
        if(b.error)return -1;
        if(kind==0){
            b.pos=(b.pos+7u)/8u*8u;
            unsigned count=get(&b,16),inv=get(&b,16);
            if(b.error||((count^inv)!=65535u)||count>OUT_BYTES-out->produced)return -1;
            while(count--){
                uint8_t v=(uint8_t)get(&b,8);
                if(b.error||monolith_sink_put(out,v))return -1;
            }
        }else if(kind==1||kind==2){
            for(;;){
                int sym=getsym(&b,&lit);
                if(sym<0)return -1;
                if(sym<256){
                    if(monolith_sink_put(out,(uint8_t)sym))return -1;
                }else if(sym==256)break;
                else{
                    unsigned idx=(unsigned)(sym-257);
                    if(idx>=29)return -1;
                    unsigned count=lens[idx]+get(&b,lextra[idx]);
                    int d=getsym(&b,&dist);
                    if(d<0||d>=30)return -1;
                    unsigned back=dists[d]+get(&b,dextra[d]);
                    if(b.error||back>out->produced||back>32768u||
                       count>OUT_BYTES-out->produced)return -1;
                    while(count--){
                        uint8_t v=out->history[(out->produced-back)&32767u];
                        if(monolith_sink_put(out,v))return -1;
                    }
                }
            }
        }else return -1;
        if(align4(&b))return -1;
        if(final)break;
    }
    return out->produced==OUT_BYTES?0:-1;
}

int monolith_resource_decode_lowmem(const uint8_t *source,size_t length,
                                    uint8_t *output,size_t out_size){
    if(!source||!output||length!=158937u||out_size<PIXEL_BYTES)
        return MONOLITH_BAD_ARGUMENT;
    if(!monolith_original_crc_ok(source,length))return MONOLITH_BAD_ARGUMENT;
    uint32_t head=(uint32_t)source[0]|((uint32_t)source[1]<<8)|
                  ((uint32_t)source[2]<<16)|((uint32_t)source[3]<<24);
    if((head&31u)!=1u||((head>>10)&2047u)!=485u||
       ((head>>21)&2047u)!=520u)return MONOLITH_BAD_ARGUMENT;
    const uint8_t *inner=source+4;
    uint32_t sz=((uint32_t)inner[0]<<24)|((uint32_t)inner[1]<<16)|
                ((uint32_t)inner[2]<<8)|inner[3];
    if(sz!=length-4u||inner[5]!=8u||inner[6]!=32u||
       inner[8]!=1u||inner[9]!=229u||inner[10]!=2u||inner[11]!=8u||
       inner[16]!=0u||inner[17]!=32u||inner[18]!=0u||inner[19]!=17u)
       return MONOLITH_BAD_ARGUMENT;
    uint32_t prev=0;
    for(unsigned i=0;i<17u;i++){
        const uint8_t *p=inner+20u+4u*i;
        uint32_t at=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
                    ((uint32_t)p[2]<<8)|p[3];
        if(at<=prev||at>=sz)return MONOLITH_BAD_ARGUMENT;
        prev=at;
    }
    uint8_t *ring=(uint8_t*)malloc(32768u);
    if(!ring)return MONOLITH_NO_MEMORY;
    monolith_sink sink={0};
    sink.history=ring;
    sink.rgb_bgr=output;
    int ok=monolith_shared_stream(source+STREAM_OFFSET,length-STREAM_OFFSET,&sink);
    free(ring);
    if(ok)return MONOLITH_BAD_ARGUMENT;
    for(size_t i=0;i<PIXEL_BYTES;i+=3u){
        uint8_t tmp=output[i];output[i]=output[i+2u];output[i+2u]=tmp;
    }
    return MONOLITH_OK;
}

/* Streamed decoding: 32768 LZ77 + 2*1455 row buffers + 1455 RGB staging.
 * Peak explicitly allocated working memory is 37133 bytes, plus small stack.
 * Caller receives each row immediately; no full framebuffer is allocated here.
 */
int monolith_resource_decode_scanlines(const uint8_t *source,size_t length,
                                       monolith_scanline_cb callback,void *context){
    if(!source||!callback||length!=158937u)
        return MONOLITH_BAD_ARGUMENT;
    if(!monolith_original_crc_ok(source,length))return MONOLITH_BAD_ARGUMENT;
    uint32_t head=(uint32_t)source[0]|((uint32_t)source[1]<<8)|
                  ((uint32_t)source[2]<<16)|((uint32_t)source[3]<<24);
    if((head&31u)!=1u||((head>>10)&2047u)!=485u||
       ((head>>21)&2047u)!=520u)return MONOLITH_BAD_ARGUMENT;
    const uint8_t *inner=source+4;
    uint32_t sz=((uint32_t)inner[0]<<24)|((uint32_t)inner[1]<<16)|
                ((uint32_t)inner[2]<<8)|inner[3];
    if(sz!=length-4u||inner[5]!=8u||inner[6]!=32u||
       inner[8]!=1u||inner[9]!=229u||inner[10]!=2u||inner[11]!=8u||
       inner[16]!=0u||inner[17]!=32u||inner[18]!=0u||inner[19]!=17u)
        return MONOLITH_BAD_ARGUMENT;
    uint32_t prev=0;
    for(unsigned i=0;i<17u;i++){
        const uint8_t *p=inner+20u+4u*i;
        uint32_t at=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
                    ((uint32_t)p[2]<<8)|p[3];
        if(at<=prev||at>=sz)return MONOLITH_BAD_ARGUMENT;
        prev=at;
    }
    const size_t stride=MONOLITH_WIDTH*3u;
    uint8_t *work=(uint8_t*)malloc(32768u+stride*3u);
    if(!work)return MONOLITH_NO_MEMORY;
    monolith_sink sink={0};
    sink.history=work;
    sink.rows=work+32768u;
    sink.rgb_row=work+32768u+stride*2u;
    sink.on_row=callback;
    sink.context=context;
    const int status=monolith_shared_stream(source+STREAM_OFFSET,
                                            length-STREAM_OFFSET,&sink);
    free(work);
    return status==0?MONOLITH_OK:MONOLITH_BAD_ARGUMENT;
}
