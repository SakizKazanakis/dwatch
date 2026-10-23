#ifndef MONOLITH_EVENT0_H
#define MONOLITH_EVENT0_H
#include <stdint.h>

/* Function-pointer types are ABI hypotheses, not OEM-published declarations. */
typedef struct monolith_lvgl_bindings {
    void *(*cont_create)(void *parent, void *copy);
    void (*obj_set_size)(void *object, int16_t width, int16_t height);
    void (*obj_align)(void *object, void *base, uint8_t align, int16_t x, int16_t y);
    void (*obj_set_parent_event)(void *object, uint8_t enabled);
    void (*page_glue_obj)(void *object, uint8_t enabled);
} monolith_lvgl_bindings;

int monolith_create_view(void *opaque_context, const monolith_lvgl_bindings *lvgl);
int monolith_dispatch_event(unsigned int event, void *opaque_context,
                            const monolith_lvgl_bindings *lvgl);
#endif
