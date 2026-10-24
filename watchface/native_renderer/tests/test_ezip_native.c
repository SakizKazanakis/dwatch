#include "monolith_renderer.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"usage: test_ezip_native original.bin decoded.rgb\n");return 2;}
    FILE *in=fopen(argv[1],"rb");if(!in)return 3;
    if(fseek(in,0,SEEK_END))return 4;
    long n=ftell(in);rewind(in);
    if(n!=158937){fclose(in);return 5;}
    unsigned char *compressed=(unsigned char*)malloc((size_t)n);
    unsigned char *rgb=(unsigned char*)malloc(485u*520u*3u);
    if(!compressed||!rgb){fclose(in);return 6;}
    if(fread(compressed,1,(size_t)n,in)!=(size_t)n){fclose(in);return 7;}
    fclose(in);
    if(monolith_resource_decode(compressed,(size_t)n,rgb,485u*520u*3u)!=MONOLITH_OK)
       return 8;
    FILE *out=fopen(argv[2],"wb");if(!out)return 9;
    if(fwrite(rgb,1,485u*520u*3u,out)!=485u*520u*3u)return 10;
    fclose(out);
    /* Do not accept truncated input or memory undersizing. */
    if(monolith_resource_decode(compressed,(size_t)n-1,rgb,485u*520u*3u)==MONOLITH_OK)return 11;
    if(monolith_resource_decode(compressed,(size_t)n,rgb,20u)==MONOLITH_OK)return 12;
    free(rgb);free(compressed);
    puts("PASS: real original eZIP decoded natively to RGB888");
    return 0;
}
