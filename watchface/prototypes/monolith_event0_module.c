/*
 * OFFLINE ONLY, NOT INSTALLABLE: manufacturer C declarations, resource pack,
 * loader policy and recovery are unverified. This file is an ABI hypothesis.
 */
#include "monolith_event0.h"
#include "monolith_design_resource.h"
#include <stdint.h>
extern char app_tileview_register_watchface;
extern char lv_cont_create, lv_obj_set_size, lv_obj_align;
extern char lv_obj_set_parent_event, lv_page_glue_obj;
extern char wf_obj_img_create, dlmodule_get_user_data;
extern char rt_malloc, rt_free, dlmodule_set_res_mng, dlmodule_get_res_mng;
extern char lv_ext_res_mng_builtin_init, lv_ext_res_mng_builtin_destroy;
extern char lv_ext_get_locale, lv_ext_set_locale, lv_i18n_lang_pack;

/* Indirect relocations match the OEM GLOB_DAT class, not the complete ABI. */
#define RESOLVE(name) ({ uintptr_t p=(uintptr_t)&name; __asm__ volatile("" : "+r"(p)); p; })
typedef int (*registration_fn)(const char*,unsigned int,const char*,void (*)(unsigned int,void*));

/* OFFLINE ABI HYPOTHESIS ONLY: exact OEM image API C prototypes unresolved. */
static int render_original_design(void *context,
                                  const monolith_design_image_descriptor *description) {
    (void) context;
    void *user = ((void *(*)(const char *, unsigned int))
                  RESOLVE(dlmodule_get_user_data))("monolith", 28u);
    if (!user) return -2;
    ((void (*)(void *, const monolith_design_image_descriptor *))
     RESOLVE(wf_obj_img_create))(user, description);
    return 0;
}

static void native_event(unsigned int event, void *ctx) {
    monolith_lvgl_bindings a;
    a.cont_create=(void *(*)(void*,void*))RESOLVE(lv_cont_create);
    a.obj_set_size=(void (*)(void*,int16_t,int16_t))RESOLVE(lv_obj_set_size);
    a.obj_align=(void (*)(void*,void*,uint8_t,int16_t,int16_t))RESOLVE(lv_obj_align);
    a.obj_set_parent_event=(void (*)(void*,uint8_t))RESOLVE(lv_obj_set_parent_event);
    a.page_glue_obj=(void (*)(void*,uint8_t))RESOLVE(lv_page_glue_obj);
    int result = monolith_dispatch_event(event,ctx,&a);
    if (event == 0 && result == 0) {
        /* This path and eZIP's firmware compatibility remain unverified.
         * The generated ELF is never safe to upload in this phase. */
        (void)monolith_attach_existing_design(ctx, render_original_design);
    }
}
__attribute__((visibility("default")))
int monolith_watchface_register(void) {
    return ((registration_fn)RESOLVE(app_tileview_register_watchface))(
        "monolith",0,"/ex/installer_wf/monolith_tn.bin",native_event);
}
__attribute__((visibility("default")))
int module_init(void *module) {
    void *mgr=((void*(*)(unsigned int))RESOLVE(rt_malloc))(44);
    if (!mgr) return -1;
    ((void(*)(void*,void*))RESOLVE(dlmodule_set_res_mng))(module,mgr);
    ((void(*)(void*,void*))RESOLVE(lv_ext_res_mng_builtin_init))(mgr,&lv_i18n_lang_pack);
    void *loc=((void*(*)(void))RESOLVE(lv_ext_get_locale))();
    ((void(*)(void*,void*))RESOLVE(lv_ext_set_locale))(mgr,loc);
    return monolith_watchface_register();
}
__attribute__((visibility("default")))
void module_cleanup(void *module) {
    void *mgr=((void*(*)(void*))RESOLVE(dlmodule_get_res_mng))(module);
    if (mgr) {
        ((void(*)(void*))RESOLVE(lv_ext_res_mng_builtin_destroy))(mgr);
        ((void(*)(void*))RESOLVE(rt_free))(mgr);
    }
}
