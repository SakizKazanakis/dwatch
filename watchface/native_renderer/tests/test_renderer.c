#include "monolith_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
  const size_t n=(size_t)MONOLITH_WIDTH*MONOLITH_HEIGHT*3u;
  unsigned char *source=(unsigned char*)malloc(n);
  if(!source)return 10;
  memset(source,17,n);
  monolith_renderer r={0};
  for(int i=0;i<1000;i++){
    if(monolith_renderer_init(&r,source,n)!=0)return 1;
    if(monolith_renderer_init(&r,source,n)==0)return 2;
    if(monolith_renderer_allocated(&r)!=n)return 3;
    if(monolith_renderer_draw(&r,11,42)!=0)return 4;
    if(monolith_renderer_pause(&r)!=0)return 5;
    if(monolith_renderer_draw(&r,11,42)==0)return 6;
    if(monolith_renderer_resume(&r)!=0)return 7;
    if(monolith_renderer_draw(&r,12,34)!=0)return 8;
    if(monolith_renderer_draw(&r,24,42)==0)return 9;
    if(!monolith_renderer_frame(&r))return 11;
    monolith_renderer_deinit(&r);
    monolith_renderer_deinit(&r);
    if(monolith_renderer_allocated(&r)!=0)return 12;
  }
  if(monolith_resource_decode(source,n,source,n)!=MONOLITH_BAD_ARGUMENT)return 13;
  if(argc>=3){
    FILE *in=fopen(argv[1],"rb");
    if(!in)return 14;
    if(fread(source,1,n,in)!=n){fclose(in);return 15;}
    fclose(in);
    if(monolith_renderer_init(&r,source,n))return 16;
    if(monolith_renderer_draw(&r,10,46))return 17;
    FILE *out=fopen(argv[2],"wb");if(!out)return 18;
    fprintf(out,"P6\n485 520\n255\n");
    if(fwrite(monolith_renderer_frame(&r),1,n,out)!=n)return 19;
    fclose(out);monolith_renderer_deinit(&r);
  }
  free(source);
  puts("PASS: 1000 native renderer lifecycle iterations, actual RGB frame draw, no double free");
  return 0;
}
