#include "monolith_oem_lifetime_mock.h"
#include <stdio.h>
#include <string.h>
static char sequence[64];
static int pos;
static int get_count;
static void *get_user(const char *key,unsigned int n) {
    if (strcmp(key,"monolith") || n!=28) return 0;
    sequence[pos++]='G';get_count++;
    return (void*)0x100;
}
static void init(void *u,void *root) {
    if(u!=(void*)0x100 || root!=(void*)0x500)sequence[pos++]='X';
    sequence[pos++]='I';
}
static void deinit(void *u) {
    sequence[pos++]=u==(void*)0x100?'D':'X';
}
static void release(const char *key) {
    sequence[pos++]=strcmp(key,"monolith")==0?'F':'X';
}
static void destroy_manager(void *m) {
    sequence[pos++]=m==(void*)0x200?'M':'X';
}
static void free_manager(void *m) {
    sequence[pos++]=m==(void*)0x200?'R':'X';
}
int main(void) {
    monolith_lifetime_hooks h={get_user,init,deinit,release,destroy_manager,free_manager};
    monolith_lifetime_state s={0,(void*)0x200,0};
    if(monolith_lifetime_open(&s,&h,(void*)0x500))return 1;
    if(monolith_lifetime_open(&s,&h,(void*)0x500)==0)return 2;
    if(monolith_lifetime_cleanup(&s,&h)==0)return 3;
    if(monolith_lifetime_close(&s,&h))return 4;
    if(monolith_lifetime_close(&s,&h)==0)return 5;
    if(monolith_lifetime_cleanup(&s,&h))return 6;
    sequence[pos]=0;
    if(strcmp(sequence,"GIDFMR")!=0)return 7;
    if(s.is_open || s.user || s.resource_manager || get_count!=1)return 8;
    printf("PASS: host-only view/resource lifecycle order %s; module cleanup blocked while open\n",sequence);
    return 0;
}
