#include "monolith_design_resource.h"
#include <stdio.h>
#include <string.h>
static int called;
static void *correct_owner;
static int mock_render(void *owner, const monolith_design_image_descriptor *d) {
    called++;
    if (owner != correct_owner || d->raw_type != 6 || d->raw_zero != 0 ||
        strcmp(d->resource_path,"/ex/resource/monolith/clock_bg.bin") != 0)
        return -5;
    return 0;
}
int main(void) {
    correct_owner=(void*)0x1234;
    if (monolith_attach_existing_design(correct_owner,mock_render)!=0 || called!=1)
       return 1;
    if (monolith_attach_existing_design(0,mock_render)==0 || called!=1) return 2;
    if (monolith_attach_existing_design(correct_owner,0)==0 || called!=1) return 3;
    if (MONOLITH_DESIGN_WIDTH!=485 || MONOLITH_DESIGN_HEIGHT!=520 ||
        MONOLITH_OEM_ROOT_WIDTH!=410 || MONOLITH_OEM_ROOT_HEIGHT!=494)
       return 4;
    puts("PASS: original design path & OEM image descriptor wired to injected renderer");
    puts("NOTE: host mock only, no proof of LVGL image rendering or scaling.");
    return 0;
}
