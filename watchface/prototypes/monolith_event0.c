/*
 * PHASE 04: OFFLINE-ONLY LVGL event-0 experiment. DO NOT UPLOAD.
 * OEM evidence: parent at context offset 0x24 and imported LVGL functions.
 * The typed C callbacks below are still hypotheses, not public OEM headers.
 */
#include "monolith_event0.h"

static int valid_api(const monolith_lvgl_bindings *a) {
    return a && a->cont_create && a->obj_set_size && a->obj_align &&
           a->obj_set_parent_event && a->page_glue_obj;
}

int monolith_create_view(void *opaque_context, const monolith_lvgl_bindings *api) {
    if (!opaque_context || !valid_api(api)) return -1;
    /* OEM's 32-bit ARM callback dereferences [context + 0x24]. */
    void *parent = *(void **)((unsigned char *)opaque_context + 0x24);
    if (!parent) return -2;

    void *root = api->cont_create(parent, 0);
    if (!root) return -3;
    api->obj_set_size(root, 410, 494);
    api->obj_align(root, parent, 0, 0, 0);
    api->obj_set_parent_event(root, 1);
    api->page_glue_obj(root, 1);

    /* A distinct Monolith graphic container; geometry is our design choice.
     * Its actual appearance depends on OEM/default LVGL styling. */
    void *panel = api->cont_create(root, 0);
    if (!panel) return -4;
    api->obj_set_size(panel, 164, 64);
    api->obj_align(panel, root, 0, 24, 24);
    return 0;
}

int monolith_dispatch_event(unsigned int event, void *context,
                            const monolith_lvgl_bindings *api) {
    if (event == 0) return monolith_create_view(context, api);
    /* Other OEM callbacks require independently verified teardown/ownership. */
    return 0;
}
