/*
 * OFFLINE HYPOTHESIS ONLY — NOT INSTALLABLE OR SAFE TO UPLOAD.
 * This executable-shaped ARM module is a linker/ABI experiment based on one
 * stock HK8 3407 watchface. Its C prototypes and OEM loader compatibility
 * are UNKNOWN; the event callback intentionally does not render a UI.
 *
 * An ordinary direct external call builds R_ARM_JUMP_SLOT PLT relocations
 * with clang/lld, unlike the OEM stock sample. The RESOLVE macro forces
 * indirect calls through R_ARM_GLOB_DAT, matching the observed relocation
 * CLASS only. Matching ELF flags/relocation classes is NOT runtime validation.
 */
#include <stdint.h>

typedef void (*wf_callback_hypothesis)(unsigned int event, void *ctx);
typedef int (*wf_register_hypothesis)(const char *, unsigned int,
                                      const char *, wf_callback_hypothesis);

extern char app_tileview_register_watchface;
extern char rt_malloc, rt_free;
extern char dlmodule_set_res_mng, dlmodule_get_res_mng;
extern char lv_ext_res_mng_builtin_init, lv_ext_res_mng_builtin_destroy;
extern char lv_ext_get_locale, lv_ext_set_locale, lv_i18n_lang_pack;

/* Compiler-only barrier: prevents synthesis of an external PLT call. */
#define RESOLVE(name) ({ uintptr_t a=(uintptr_t)&name; __asm__ volatile("" : "+r"(a)); a; })

static void monolith_event_callback(unsigned int event, void *ctx) {
    (void) event;
    (void) ctx;
    /* No display. OEM callback context and UI setup are not established. */
}

__attribute__((visibility("default")))
int monolith_watchface_register(void) {
    return ((wf_register_hypothesis) RESOLVE(app_tileview_register_watchface))(
        "monolith", 0, "/ex/installer_wf/monolith_tn.bin",
        monolith_event_callback);
}

__attribute__((visibility("default")))
int module_init(void *module) {
    void *manager = ((void *(*)(unsigned int)) RESOLVE(rt_malloc))(44);
    if (!manager) return -1; /* Added safeguard, NOT an observed OEM branch. */
    ((void (*)(void*,void*)) RESOLVE(dlmodule_set_res_mng))(module, manager);
    ((void (*)(void*,void*)) RESOLVE(lv_ext_res_mng_builtin_init))(
        manager, &lv_i18n_lang_pack);
    void *locale = ((void *(*)(void)) RESOLVE(lv_ext_get_locale))();
    ((void (*)(void*,void*)) RESOLVE(lv_ext_set_locale))(manager, locale);
    return monolith_watchface_register();
}

__attribute__((visibility("default")))
void module_cleanup(void *module) {
    void *manager = ((void *(*)(void*)) RESOLVE(dlmodule_get_res_mng))(module);
    if (manager) {
        ((void (*)(void*)) RESOLVE(lv_ext_res_mng_builtin_destroy))(manager);
        ((void (*)(void*)) RESOLVE(rt_free))(manager);
    }
}
