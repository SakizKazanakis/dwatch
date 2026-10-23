/*
 * Independent MONOLITH legacy SiFli eZIP shared-Huffman decoder.
 * Algorithm based on RFC 1951 canonical Huffman, LZ77 and PNG row filters;
 * real-device eZIP container/4-byte alignment reconstructed from original BIN.
 * No proprietary OEM routines, firmware/OTA code or third-party decoder copied.
 * Does NOT establish watch bootloader/RT-Thread compatibility.
 */
#include "monolith_renderer.h"
#include <stdlib.h>
#include <string.h>

#define HUFF_MAX 320
#define IMAGE_MAX_PIXELS 4190209u
typedef struct {
    const uint8_t *p;
    size_t size;
    size_t bit;
} BitReader;
typedef struct {
    uint16_t count[16],first_code[16],first_index[16];
    uint16_t symbols[HUFF_MAX];
    uint16_t total;
} Huff;

static uint32_t u32le(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint16_t u16be(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0]<<8)|p[1];
}
static uint32_t u32be(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static int bitread(BitReader *b,unsigned n,unsigned *value) {
    if(n>16||b->bit > b->size*8 || n > b->size*8-b->bit)return MONOLITH_DECODE_FAILED;
    unsigned out=0;
    for(unsigned i=0;i<n;i++) {
        size_t pos=b->bit+i;
        out|=(unsigned)((b->p[pos>>3]>>(pos&7))&1u)<<i;
    }
    b->bit+=n;*value=out;return MONOLITH_OK;
}
static int align4(BitReader *b) {
    size_t byte=(b->bit+7)/8u;
    if(byte>SIZE_MAX-3u)return MONOLITH_DECODE_FAILED;
    byte=(byte+3u)&~(size_t)3u;
    if(byte>b->size)return MONOLITH_DECODE_FAILED;
    b->bit=byte*8u;return MONOLITH_OK;
}
static int build_huffman(Huff *dst,const uint8_t *lengths,unsigned n) {
    if(!dst||!lengths||!n||n>HUFF_MAX)return MONOLITH_INVALID;
    memset(dst,0,sizeof(*dst));
    unsigned next[16]={0};
    for(unsigned i=0;i<n;i++){
        if(lengths[i]>15)return MONOLITH_DECODE_FAILED;
        if(lengths[i])dst->count[lengths[i]]++;
    }
    unsigned code=0,index=0;
    for(unsigned bits=1;bits<=15;bits++) {
        code=(code+dst->count[bits-1])*2u;
        if(code+dst->count[bits]>(1u<<bits))return MONOLITH_DECODE_FAILED;
        dst->first_code[bits]=(uint16_t)code;
        dst->first_index[bits]=(uint16_t)index;
        next[bits]=index;
        index+=dst->count[bits];
    }
    if(!index)return MONOLITH_DECODE_FAILED;
    dst->total=(uint16_t)index;
    for(unsigned sym=0;sym<n;sym++){
        const unsigned len=lengths[sym];
        if(len)dst->symbols[next[len]++]=(uint16_t)sym;
    }
    return MONOLITH_OK;
}
static int read_symbol(BitReader *b,const Huff *tree,unsigned *out) {
    unsigned code=0;
    for(unsigned width=1;width<=15;width++){
        unsigned bit=0;
        if(bitread(b,1,&bit))return MONOLITH_DECODE_FAILED;
        code=(code<<1)|bit;
        if(tree->count[width] &&
           code>=tree->first_code[width] &&
           code-tree->first_code[width]<tree->count[width]){
            *out=tree->symbols[tree->first_index[width]+code-tree->first_code[width]];
            return MONOLITH_OK;
        }
    }
    return MONOLITH_DECODE_FAILED;
}
static int read_shared_tables(BitReader *b,Huff *literal,Huff *distance) {
    static const uint8_t order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    unsigned ll=0,dd=0,cl=0;
    if(bitread(b,5,&ll)||bitread(b,5,&dd)||bitread(b,4,&cl))return MONOLITH_DECODE_FAILED;
    ll+=257;dd+=1;cl+=4;
    if(ll>286||dd>32||cl>19)return MONOLITH_DECODE_FAILED;
    uint8_t code_lengths[19]={0},all_lengths[HUFF_MAX]={0};
    for(unsigned i=0;i<cl;i++){
        unsigned len=0;if(bitread(b,3,&len))return MONOLITH_DECODE_FAILED;
        code_lengths[order[i]]=(uint8_t)len;
    }
    Huff codebook;
    if(build_huffman(&codebook,code_lengths,19))return MONOLITH_DECODE_FAILED;
    unsigned have=0,required=ll+dd;
    while(have<required){
        unsigned tok=0;if(read_symbol(b,&codebook,&tok))return MONOLITH_DECODE_FAILED;
        if(tok<=15)all_lengths[have++]=(uint8_t)tok;
        else {
            unsigned extra=0,count=0,value=0;
            if(tok==16){
                if(!have||bitread(b,2,&extra))return MONOLITH_DECODE_FAILED;
                count=extra+3;value=all_lengths[have-1];
            }else if(tok==17){
                if(bitread(b,3,&extra))return MONOLITH_DECODE_FAILED;
                count=extra+3;
            }else if(tok==18){
                if(bitread(b,7,&extra))return MONOLITH_DECODE_FAILED;
                count=extra+11;
            }else return MONOLITH_DECODE_FAILED;
            if(count>required-have)return MONOLITH_DECODE_FAILED;
            memset(all_lengths+have,(int)value,count);have+=count;
        }
    }
    if(build_huffman(literal,all_lengths,ll))return MONOLITH_DECODE_FAILED;
    if(build_huffman(distance,all_lengths+ll,dd))return MONOLITH_DECODE_FAILED;
    return align4(b);
}
static const uint16_t lengthbase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t lengthextra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t distbase[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t distextra[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int inflate_shared(const uint8_t *data,size_t len,size_t shared_len,
                          uint8_t *output,size_t capacity,size_t *written) {
    BitReader b={data,len,0};
    Huff literals,dists;
    if(read_shared_tables(&b,&literals,&dists))return MONOLITH_DECODE_FAILED;
    if(b.bit/8!=shared_len)return MONOLITH_DECODE_FAILED;
    size_t produced=0;unsigned block=0,finished=0;
    while(!finished){
        unsigned final=0,typ=0;
        if(bitread(&b,1,&final)||bitread(&b,2,&typ))return MONOLITH_DECODE_FAILED;
        if(++block>8192)return MONOLITH_DECODE_FAILED;
        if(typ==0){
            b.bit=(b.bit+7)&~(size_t)7;
            unsigned n=0,inv=0;
            if(bitread(&b,16,&n)||bitread(&b,16,&inv)||(n^inv)!=65535u)return MONOLITH_DECODE_FAILED;
            if(produced>capacity||n>capacity-produced ||
               b.bit/8>len||n>len-b.bit/8)return MONOLITH_DECODE_FAILED;
            memcpy(output+produced,data+b.bit/8,n);produced+=n;b.bit+=8u*n;
        } else if(typ==1||typ==2) {
            for(;;){
                unsigned sym=0;
                if(read_symbol(&b,&literals,&sym))return MONOLITH_DECODE_FAILED;
                if(sym<256){
                    if(produced>=capacity)return MONOLITH_DECODE_FAILED;
                    output[produced++]=(uint8_t)sym;
                }else if(sym==256)break;
                else if(sym>=257&&sym<=285) {
                    unsigned i=sym-257,ex=0,dcode=0,distex=0;
                    if(bitread(&b,lengthextra[i],&ex) ||
                       read_symbol(&b,&dists,&dcode) || dcode>=30 ||
                       bitread(&b,distextra[dcode],&distex))return MONOLITH_DECODE_FAILED;
                    const size_t count=(size_t)lengthbase[i]+ex;
                    const size_t dist=(size_t)distbase[dcode]+distex;
                    if(dist>produced||count>capacity-produced)return MONOLITH_DECODE_FAILED;
                    for(size_t j=0;j<count;j++){
                        output[produced]=output[produced-dist];produced++;
                    }
                }else return MONOLITH_DECODE_FAILED;
            }
        }else return MONOLITH_DECODE_FAILED;
        if(align4(&b))return MONOLITH_DECODE_FAILED;
        if(final)finished=1;
    }
    *written=produced;
    return MONOLITH_OK;
}
static int unfilter_bgr_to_rgb(const uint8_t *input,size_t size,
                              uint8_t *rgb,unsigned w,unsigned h,unsigned row_group) {
    const size_t stride=(size_t)w*3u;
    if(size!=(stride+1u)*h||!row_group)return MONOLITH_DECODE_FAILED;
    for(unsigned row=0;row<h;row++){
        size_t off=(stride+1u)*row;
        unsigned f=input[off];
        if(f>4)return MONOLITH_DECODE_FAILED;
        const int boundary=(row%row_group)==0;
        for(size_t k=0;k<stride;k++){
            const unsigned channel=(unsigned)(k%3u);
            const size_t mapped=(k/3u)*3u+2u-channel;
            const size_t out=(size_t)row*stride+mapped;
            const unsigned left=k>=3?rgb[out-3u]:0;
            const unsigned above=boundary?0:rgb[out-stride];
            const unsigned corner=(boundary||k<3)?0:rgb[out-stride-3u];
            unsigned prediction=0;
            if(f==1)prediction=left;
            else if(f==2)prediction=above;
            else if(f==3)prediction=(left+above)/2u;
            else if(f==4) {
                const int p=(int)left+(int)above-(int)corner;
                const int a=(p>(int)left)?(p-(int)left):((int)left-p);
                const int b=(p>(int)above)?(p-(int)above):((int)above-p);
                const int c=(p>(int)corner)?(p-(int)corner):((int)corner-p);
                prediction=(a<=b&&a<=c)?left:((b<=c)?above:corner);
            }
            rgb[out]=(uint8_t)((input[off+1+k]+prediction)&255u);
        }
    }
    return MONOLITH_OK;
}

int monolith_resource_open(monolith_resource *r,const uint8_t *data,size_t size) {
    if(!r||!data)return MONOLITH_INVALID;
    memset(r,0,sizeof(*r));
    if(size<36||size>MONOLITH_RESOURCE_MAX_BYTES)return MONOLITH_INVALID;
    uint32_t head=u32le(data);
    const unsigned fmt=head&31u,w=(head>>10)&2047u,h=(head>>21)&2047u;
    if(!w||!h||w>MONOLITH_IMG_MAX_W||h>MONOLITH_IMG_MAX_H ||
       (size_t)w*h>IMAGE_MAX_PIXELS)return MONOLITH_INVALID;
    if(fmt!=MONOLITH_FORMAT_BGR888_EZIP||data[9]!=8)return MONOLITH_UNSUPPORTED;
    const uint32_t declared=u32be(data+4);
    const unsigned rows=data[10], hw=u16be(data+12),hh=u16be(data+14);
    const unsigned table_rows=u16be(data+20),n=u16be(data+22);
    if(declared!=size-4||w!=hw||h!=hh||!rows||rows>h||
       table_rows!=rows||n==0||n>8192||n!=(h+rows-1)/rows)return MONOLITH_INVALID;
    const size_t table_end=24+(size_t)n*4u;
    if(table_end>=size)return MONOLITH_INVALID;
    uint32_t last=0,first=0;
    for(unsigned i=0;i<n;i++){
        uint32_t v=u32be(data+24+4u*i);
        if(i==0)v&=0x00ffffffu;
        if(v<=table_end-4||v>=size-4||(i&&v<=last))return MONOLITH_INVALID;
        if(!i)first=v;
        last=v;
    }
    r->compressed=data;r->compressed_bytes=size;r->width=(uint16_t)w;
    r->height=(uint16_t)h;r->row_height=(uint16_t)rows;r->block_count=(uint16_t)n;
    r->color_format=(uint8_t)fmt;r->bit_depth=data[9];
    r->table_end=(uint32_t)(table_end-4);r->first_block=first;
    return MONOLITH_OK;
}
int monolith_resource_decode(monolith_resource *r) {
    if(!r||!r->compressed||r->rgb)return MONOLITH_STATE;
    const size_t raw=(size_t)r->width*r->height*3u;
    const size_t filtered_capacity=raw+r->height;
    uint8_t *filtered=malloc(filtered_capacity);
    if(!filtered)return MONOLITH_NOMEM;
    uint8_t *pixels=malloc(raw);
    if(!pixels){free(filtered);return MONOLITH_NOMEM;}
    size_t expanded=0;
    const uint8_t *base=r->compressed+4;
    int rc=inflate_shared(base+r->table_end,r->compressed_bytes-4-r->table_end,
                          r->first_block-r->table_end,filtered,filtered_capacity,&expanded);
    if(!rc && expanded!=filtered_capacity)rc=MONOLITH_DECODE_FAILED;
    if(!rc)rc=unfilter_bgr_to_rgb(filtered,expanded,pixels,r->width,r->height,r->row_height);
    free(filtered);
    if(rc){free(pixels);return rc;}
    r->rgb=pixels;r->rgb_size=raw;
    r->peak_memory_bytes=raw+filtered_capacity;
    return MONOLITH_OK;
}
void monolith_resource_release(monolith_resource *r) {
    if(!r)return;
    free(r->rgb);
    memset(r,0,sizeof(*r));
}
const char *monolith_status_string(int s) {
    switch(s){
    case MONOLITH_OK:return "OK";
    case MONOLITH_INVALID:return "INVALID RESOURCE";
    case MONOLITH_UNSUPPORTED:return "UNSUPPORTED";
    case MONOLITH_NOMEM:return "OUT OF MEMORY";
    case MONOLITH_DECODE_FAILED:return "DECODE FAILED";
    case MONOLITH_STATE:return "INVALID STATE";
    default:return "UNKNOWN ERROR";
    }
}
