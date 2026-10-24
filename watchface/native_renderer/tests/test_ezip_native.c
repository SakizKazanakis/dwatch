#include "monolith_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
int main(int argc,char **argv){
    if(argc!=3 && argc!=4){fprintf(stderr,"usage: test_ezip_native original.bin decoded.rgb [lowmem]\n");return 2;}
    FILE *in=fopen(argv[1],"rb");if(!in)return 3;
    if(fseek(in,0,SEEK_END))return 4;
    long n=ftell(in);rewind(in);
    if(n!=158937){fclose(in);return 5;}
    unsigned char *compressed=(unsigned char*)malloc((size_t)n);
    unsigned char *rgb=(unsigned char*)malloc(485u*520u*3u);
    if(!compressed||!rgb){fclose(in);return 6;}
    if(fread(compressed,1,(size_t)n,in)!=(size_t)n){fclose(in);return 7;}
    fclose(in);
    if((argc==4 ? monolith_resource_decode_lowmem : monolith_resource_decode)(compressed,(size_t)n,rgb,485u*520u*3u)!=MONOLITH_OK)
       return 8;
    FILE *out=fopen(argv[2],"wb");if(!out)return 9;
    if(fwrite(rgb,1,485u*520u*3u,out)!=485u*520u*3u)return 10;
    fclose(out);
    /* Do not accept truncated input or memory undersizing. */
    if((argc==4 ? monolith_resource_decode_lowmem : monolith_resource_decode)(compressed,(size_t)n-1,rgb,485u*520u*3u)==MONOLITH_OK)return 11;
    if((argc==4 ? monolith_resource_decode_lowmem : monolith_resource_decode)(compressed,(size_t)n,rgb,20u)==MONOLITH_OK)return 12;
    /* Real-source adversarial checks: altered bytes MUST be rejected before
       the output buffer is touched (both supported native decoder modes). */
    const unsigned offsets[] = {0u,3u,4u,16u,31u,88u,92u,208u,4096u,158936u};
    for(unsigned mode=0;mode<2;mode++){
        int (*fn)(const uint8_t*,size_t,uint8_t*,size_t)=
            mode?monolith_resource_decode_lowmem:monolith_resource_decode;
        for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){
            unsigned pos=offsets[i];
            uint8_t saved=compressed[pos];
            compressed[pos]^=0x5au;
            memset(rgb,0xa5,485u*520u*3u);
            if(fn(compressed,(size_t)n,rgb,485u*520u*3u)==MONOLITH_OK)return 20;
            for(size_t j=0;j<485u*520u*3u;j++)
                if(rgb[j]!=0xa5)return 21;
            compressed[pos]=saved;
        }
        memset(rgb,0xa5,485u*520u*3u);
        if(fn(compressed,(size_t)n,rgb,485u*520u*3u)==MONOLITH_OK){
            /* Genuine input is expected to succeed and fill the buffer. */
        }else return 22;
    }
    free(rgb);free(compressed);
    puts("PASS: real original eZIP decoded natively to RGB888");
    return 0;
}
