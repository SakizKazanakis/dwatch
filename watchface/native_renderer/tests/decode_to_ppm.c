#include "monolith_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_all(const char *path,uint8_t **data,size_t *n) {
    FILE *f=fopen(path,"rb");
    if(!f)return -1;
    if(fseek(f,0,SEEK_END)) {fclose(f);return -1;}
    long size=ftell(f);
    if(size<=0||size>MONOLITH_RESOURCE_MAX_BYTES||fseek(f,0,SEEK_SET)){
        fclose(f);return -1;
    }
    uint8_t *p=(uint8_t *)malloc((size_t)size);
    if(!p){fclose(f);return -1;}
    size_t r=fread(p,1,(size_t)size,f);
    fclose(f);
    if(r!=(size_t)size){free(p);return -1;}
    *data=p;*n=r;return 0;
}
static int write_ppm(const char *name,unsigned w,unsigned h,const uint8_t *data,size_t size){
    FILE *f=fopen(name,"wb");
    if(!f)return -1;
    fprintf(f,"P6\n%u %u\n255\n",w,h);
    int rc=fwrite(data,1,size,f)!=size;
    if(fclose(f))rc=-1;
    return rc;
}
int main(int argc,char **argv){
    if(argc!=3){
        fprintf(stderr,"usage: monolith_decode_native SOURCE_BIN OUT_PPM\n");
        return 2;
    }
    uint8_t *source=0;size_t src_size=0;
    if(read_all(argv[1],&source,&src_size)){
        fprintf(stderr,"cannot open input\n");return 2;
    }
    monolith_resource resource;
    int rc=monolith_resource_open(&resource,source,src_size);
    if(!rc)rc=monolith_resource_decode(&resource);
    if(rc){
        fprintf(stderr,"decode failed: %s\n",monolith_status_string(rc));
        if(resource.compressed)monolith_resource_release(&resource);
        free(source);return 1;
    }
    if(write_ppm(argv[2],resource.width,resource.height,resource.rgb,resource.rgb_size)){
        fprintf(stderr,"cannot write PPM\n");
        monolith_resource_release(&resource);free(source);return 1;
    }
    fprintf(stdout,"PASS native eZIP: %ux%u, %zu decoded bytes, %u blocks, "
                   "peak decoder owned RAM %zu B\n",
            resource.width,resource.height,resource.rgb_size,resource.block_count,
            resource.peak_memory_bytes);
    monolith_resource_release(&resource);
    free(source);
    return 0;
}
