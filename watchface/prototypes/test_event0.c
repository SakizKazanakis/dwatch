#include "monolith_event0.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int calls, bad, flags, glued, align_root, align_panel;
static int width_root,height_root,width_panel,height_panel;
static void *expected_parent;
static void *root = (void *)(uintptr_t)0x1111;
static void *panel = (void *)(uintptr_t)0x2222;

static void *create(void *parent, void *copy) {
    if (copy != NULL) bad++;
    if (calls == 0 && parent != expected_parent) bad++;
    if (calls == 1 && parent != root) bad++;
    if (calls++ > 1) bad++;
    return calls == 1 ? root : panel;
}
static void size(void *obj, int16_t w, int16_t h) {
    if (obj == root) { width_root = w; height_root = h; }
    else if (obj == panel) { width_panel = w; height_panel = h; }
    else bad++;
}
static void align(void *obj, void *base, uint8_t a, int16_t x, int16_t y) {
    if (a != 0) bad++;
    if (obj == root) {
        if (base != expected_parent || x != 0 || y != 0) bad++;
        align_root++;
    } else if (obj == panel) {
        if (base != root || x != 24 || y != 24) bad++;
        align_panel++;
    } else bad++;
}
static void parent_event(void *obj, uint8_t enabled) {
    if (obj != root || enabled != 1) bad++;
    flags++;
}
static void glue(void *obj, uint8_t enabled) {
    if (obj != root || enabled != 1) bad++;
    glued++;
}
static monolith_lvgl_bindings api = {create,size,align,parent_event,glue};

int main(void) {
    /* Pointer is explicitly stored at byte 0x24; ordinary host struct
       padding on 64-bit Linux would invalidate that physical ARM offset. */
    unsigned char ctx[0x24 + sizeof(void *)];
    memset(ctx,0,sizeof(ctx));
    expected_parent=(void *)(uintptr_t)0x3456;
    memcpy(ctx+0x24,&expected_parent,sizeof(expected_parent));
    if (monolith_dispatch_event(0,ctx,&api) != 0) return 1;
    if (bad || calls != 2 || width_root != 410 || height_root != 494 ||
        width_panel != 164 || height_panel != 64 || align_root != 1 ||
        align_panel != 1 || flags != 1 || glued != 1) {
        fprintf(stderr,"FAILED host event0 mock: bad=%d calls=%d\n",bad,calls);
        return 1;
    }
    int before=calls;
    if (monolith_dispatch_event(2,ctx,&api) != 0 || calls != before) return 1;
    if (monolith_dispatch_event(0,0,&api) == 0 || calls != before) return 1;
    memset(ctx+0x24,0,sizeof(expected_parent));
    if (monolith_dispatch_event(0,ctx,&api) == 0 || calls != before) return 1;
    puts("PASS: LVGL event0: root 410x494, own panel 164x64, parent flags and invalid context");
    return 0;
}
