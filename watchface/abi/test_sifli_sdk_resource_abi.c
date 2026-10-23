#include "sifli_sdk_resource_abi.h"
#include <stddef.h>

/* Compile for the original ARM32 target. Both assertions derive from the
 * Apache-2.0 OpenSiFli SDK, NOT guessed watch-specific firmware internals. */
_Static_assert(sizeof(sifli_lv_res_mng_t) == 44, "OEM rt_malloc(44) mismatch");
_Static_assert(offsetof(sifli_lv_res_mng_t, curr_lang) == 0, "curr_lang");
_Static_assert(offsetof(sifli_lv_res_mng_t, curr_lang_node) == 4, "curr_lang_node");
_Static_assert(offsetof(sifli_lv_res_mng_t, lang_pack_list) == 8, "lang_pack_list");
_Static_assert(offsetof(sifli_lv_res_mng_t, load) == 20, "load hook");
_Static_assert(offsetof(sifli_lv_res_mng_t, get_img) == 24, "image hook");
_Static_assert(offsetof(sifli_lv_res_mng_t, get_string) == 28, "string hook");
_Static_assert(offsetof(sifli_lv_res_mng_t, user_data) == 32, "user data");
_Static_assert(offsetof(sifli_lv_res_mng_t, get_lang_pack) == 36, "lang packs");
_Static_assert(offsetof(sifli_lv_res_mng_t, get_font_config) == 40, "fonts");
_Static_assert(sizeof(sifli_lv_ll_t) == 12, "lv_ll_t 32-bit");

int sifli_abi_layout_test(void) { return sizeof(sifli_lv_res_mng_t); }
