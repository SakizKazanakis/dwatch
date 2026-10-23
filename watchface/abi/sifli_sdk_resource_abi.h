/* SPDX-License-Identifier: Apache-2.0
 * ABI size/layout-only transcription of OpenSiFli public SDK structures:
 * - middleware/lvgl/lv_ext_resouce/lv_ext_resource_manager.h
 * - external/lvgl_v8/src/misc/lv_ll.h
 * - external/lvgl_v8/src/misc/lv_types.h
 *
 * This is NOT a firmware-matched manufacturer header and no watch write is
 * permitted. Structure offsets are verified for a 32-bit ARM build only.
 */
#ifndef MONOLITH_SIFLI_SDK_RESOURCE_ABI_H
#define MONOLITH_SIFLI_SDK_RESOURCE_ABI_H
#include <stddef.h>
#include <stdint.h>

typedef uint8_t sifli_lv_res_t;

typedef struct {
    const char *locale;
    const void *translation;
} sifli_lv_i18n_lang_t;
typedef const sifli_lv_i18n_lang_t *sifli_lv_i18n_lang_pack_t;

typedef struct {
    uint32_t n_size;
    void *head;
    void *tail;
} sifli_lv_ll_t;

struct sifli_lv_ext_res_mng_tag;
typedef struct sifli_lv_ext_res_mng_tag *sifli_lv_ext_res_mng_ptr;

/*
 * Field order and function-pointer slots match public SiFli-SDK:
 * struct lv_ext_res_mng_tag. The opaque image/font definitions deliberately
 * do not pretend to specify firmware-compatible LVGL structs.
 */
typedef struct sifli_lv_ext_res_mng_tag {
    const sifli_lv_i18n_lang_t *curr_lang;
    const void *curr_lang_node;
    sifli_lv_ll_t lang_pack_list;
    sifli_lv_res_t (*load)(sifli_lv_ext_res_mng_ptr manager);
    void *(*get_img)(sifli_lv_ext_res_mng_ptr manager, const char *key);
    const char *(*get_string)(sifli_lv_ext_res_mng_ptr manager, uint32_t offset, const char *key);
    void *user_data;
    const sifli_lv_i18n_lang_pack_t *(*get_lang_pack)(sifli_lv_ext_res_mng_ptr manager);
    const void *(*get_font_config)(sifli_lv_ext_res_mng_ptr manager);
} sifli_lv_res_mng_t;

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(sifli_lv_res_mng_t) == 44,
               "The public SiFli LVGL resource-manager ABI is not 44 bytes");
_Static_assert(offsetof(sifli_lv_res_mng_t, user_data) == 32,
               "Public SDK resource manager slot offset mismatch");
#endif
#endif
