# PHASE 04 — matched SiFli public SDK resource ABI vs OEM HK8 3407 (2026-10-23)

## Executive result

A substantial portion of the *original module manager and user-data ABI* has now been recovered from real public **Apache-2.0 SiFli-SDK C source**, rather than guessing from ARM register values. The symbol `wf_obj_img_create` remains proprietary: an exact implementation or firmware-matched C declaration was **not** found. The tested OpenSiFli SDK demonstrates an **alternative** LVGL/eZIP decoder API, but does not prove it is the code deployed in HK8 PRO MAX / HK ONE device 3407 firmware 2.09.

All observations below distinguish (A) recovered HK8 OEM ARM module, (B) official *public* SDK source, and (C) an implementation inference.

## 1. Source map (actual GitHub C files, not search-result guesses)

1. [OpenSiFli/SiFli-SDK / middleware/include/mod_installer.h](https://github.com/OpenSiFli/SiFli-SDK/blob/main/middleware/include/mod_installer.h) — source definitions for `MODULE_INIT_DEF`, `MODULE_CLEANUP_DEF`, `MODULE_CONTEXT` and `MODULE_CONTEXT_FREE`.
2. [middleware/lvgl/lv_ext_resouce/lv_ext_resource_manager.h](https://github.com/OpenSiFli/SiFli-SDK/blob/main/middleware/lvgl/lv_ext_resouce/lv_ext_resource_manager.h) — complete `struct lv_ext_res_mng_tag`, `lv_i18n_lang_pack_t`, `lv_ext_set_locale` and `lv_ext_get_locale` prototypes; `LV_EXT_IMG_GET` maps to `/ex/resource/*.bin` when compiled with `LV_USING_FILE_RESOURCE` and without `SOLUTION_WATCH`.
3. [middleware/lvgl/lv_ext_resouce/lv_ext_resource_manager_builtin.c](https://github.com/OpenSiFli/SiFli-SDK/blob/main/middleware/lvgl/lv_ext_resouce/lv_ext_resource_manager_builtin.c) — actual `lv_ext_res_mng_builtin_init` and `lv_ext_res_mng_builtin_destroy` implementations. **These set up and clear language-pack data, not a universal image decoder.**
4. [external/lvgl_v8/src/misc/lv_ll.h](https://github.com/OpenSiFli/SiFli-SDK/blob/main/external/lvgl_v8/src/misc/lv_ll.h) — `lv_ll_t` layout `uint32_t n_size; void* head; void* tail`; 12 bytes on ARM32.
5. [external/lvgl_v8/src/misc/lv_types.h](https://github.com/OpenSiFli/SiFli-SDK/blob/main/external/lvgl_v8/src/misc/lv_types.h) — `lv_res_t` is `uint8_t`.
6. [rtos/rtthread/components/libc/libdl/dlmodule.h](https://github.com/OpenSiFli/SiFli-SDK/blob/main/rtos/rtthread/components/libc/libdl/dlmodule.h) and [dlmodule.c](https://github.com/OpenSiFli/SiFli-SDK/blob/main/rtos/rtthread/components/libc/libdl/dlmodule.c) — actual `dlmodule_get_user_data`, `dlmodule_free_user_data`, `dlmodule_set_res_mng`, `dlmodule_get_res_mng`.
7. [middleware/lvgl/lv_drivers/lvsf_img_decoder.c](https://github.com/OpenSiFli/SiFli-SDK/blob/main/middleware/lvgl/lv_drivers/lvsf_img_decoder.c) — real file-image decoder lifecycle `file_decoder_info/open/read_line/close` registered through `lv_sifli_img_decoder`.
8. [example/multimedia/lvgl/ezip/src/main.c](https://github.com/OpenSiFli/SiFli-SDK/blob/main/example/multimedia/lvgl/ezip/src/main.c) — working **reference** example with `load_ezip`, `unload_ezip`, `lv_img_set_src`; supports file mode and explicit preload into SRAM or PSRAM.
9. [example/multimedia/lvgl/watch/src/gui_apps/clock/app_clock_dial.c](https://github.com/OpenSiFli/SiFli-SDK/blob/main/example/multimedia/lvgl/watch/src/gui_apps/clock/app_clock_dial.c) — actual sample dial calls `lv_img_create`, `lv_img_set_src`, `lv_img_decoder_get_info`, `lv_img_set_zoom` and handles rotation-image cache.
10. [SiFli eZIP conversion documentation](https://docs.sifli.com/projects/sdk/latest/en/sf32lb52x/app_note/ezip_tool_usage.html) — four-byte resource header and format codes 1 (compressed RGB) / 2 (compressed RGB with alpha).

The public SDK files have actual code, but `lv_cont_create` and `wf_obj_img_create` are **not found** in the current public OpenSiFli main branch. The HK8 OEM ELF nevertheless **imports** both. This is meaningful evidence of a firmware/library version or vendor-specific extension mismatch. Do not attribute public LVGL-v8 functions directly to the HK8 ROM without further verification.

## 2. Exactly matching module-entry and memory-management ABI

The SDK macro defines **`void module_init(struct rt_dlmodule *module)`**, *not* `int module_init(void*)`. Its sequence is:

```c
void *manager = rt_malloc(sizeof(struct lv_ext_res_mng_tag));
dlmodule_set_res_mng(module, manager);
lv_ext_res_mng_builtin_init(manager, lv_i18n_lang_pack);
lv_ext_set_locale(manager, lv_ext_get_locale());
/* optional custom init() */
```

The corresponding `MODULE_CLEANUP_DEF` declares `void module_cleanup(struct rt_dlmodule*)`, calls `dlmodule_get_res_mng`, then (conditionally) `lv_ext_res_mng_builtin_destroy` and `rt_free`. The observed OEM `wf_clock23.so` exposes these names in essentially the same order and requests **44 bytes** for the manager.

The *public* `lv_ext_res_mng_tag` has two initial pointers (8 bytes), a 12-byte `lv_ll_t`, and six additional pointer-sized fields (24 bytes): **8 + 12 + 24 = 44 bytes** on 32-bit ARM. This matches the captured `rt_malloc(44)` literal exactly. `lv_ext_res_mng_builtin_init(manager,lang_pack)` initializes the language linked list and sets `curr_lang`; `...destroy(manager)` clears the language list and its references.

**Recovered public prototypes:**

```c
lv_res_t lv_ext_res_mng_builtin_init(
    lv_ext_res_mng_t manager, const lv_i18n_lang_pack_t *pack);
lv_res_t lv_ext_res_mng_builtin_destroy(lv_ext_res_mng_t manager);
lv_res_t lv_ext_set_locale(lv_ext_res_mng_t manager, const char *locale);
const char *lv_ext_get_locale(void);
void *dlmodule_get_user_data(const char *module_name, rt_uint32_t size);
void dlmodule_free_user_data(const char *module_name);
void *dlmodule_get_res_mng(struct rt_dlmodule *module);
void dlmodule_set_res_mng(struct rt_dlmodule *module, void *manager);
```

`dlmodule.c` explains the observed repeated calls requesting 28 bytes: it searches the named module, allocates its `user_data` **only on first request** with `dlm_malloc(size)`, caches the pointer on the module, and returns the same pointer subsequently. `dlmodule_free_user_data` calls `dlm_free` and resets both `user_data` and `user_data_size`. The separate manager allocation is independent of this 28-byte user data. This behaviour is **source-confirmed for public SDK** and compatible with the observed OEM call pattern, but its precise presence in the installed firmware is unverified.

The reconstructed OEM resource `wf_clock23_res.so` **exports `lv_i18n_lang_pack` as a 108-byte object** (27 pointer slots on ARM32). A standalone Monolith watchface needs its **own compatible locale/resource module**, not an invented zero-length array.

## 3. What is actually known about the image decoder

A generic, publicly implemented SiFli LVGL file decoder does the following:

1. In `file_decoder_info`, opens the image and reads an LVGL header containing width, height and image format.
2. In `file_decoder_open`, obtains file size, excludes the LVGL header and either **direct-maps eligible NOR/MTD memory** or allocates a buffer using `app_cache_alloc(...,IMAGE_CACHE_PSRAM)` and reads the image data.
3. Marks locally allocated memory with `dsc->user_data = dsc->img_data`.
4. In `file_decoder_close`, frees through `app_cache_free` **only when locally owned**; direct-mapped data is not freed as allocated memory.
5. `lv_sifli_img_decoder` registers the callbacks. The exact GPU/eZIP decompression path is below this level and depends on the configured SDK and hardware.

The official dynamic example provides another real code path, `load_ezip` and `unload_ezip`, which preloads compressed bytes to SRAM/PSRAM and passes the resulting LVGL descriptor to `lv_img_set_src`. File mode instead passes the filesystem path directly. The public watch clock example explicitly applies `lv_img_set_zoom` to match a source image to its parent.

**CRITICAL:** these are implementations from the public SiFli SDK, not the *actual* body of HK8 `wf_obj_img_create`. No `wf_obj_img_create` implementation or matching header was found in public OpenSiFli main. Do not claim that the watch's proprietary image helper direct-maps NOR, does not crop, or uses this particular zoom policy. The captured OEM `wf_clock23` face provides only the call site, not the implementation.

## 4. Changes implemented in Monolith repo

- `watchface/abi/sifli_sdk_resource_abi.h` — namespaced **offline ABI mirror** of the public manager layout; all original manufacturer firmware assumptions remain explicit.
- `watchface/abi/test_sifli_sdk_resource_abi.c` — ARM32 `_Static_assert` tests verify size 44 and all 9 offsets, including `user_data` at byte 32.
- `watchface/abi/check_resource_abi.sh` — compiles those assertions with Clang for ARMv7 hard-float and checks object architecture; CI runs it.
- Corrected experimental `module_init` and `module_cleanup` to **void** functions in previous native ABI stubs, including `monolith_event0_module.c`; the manager allocation is `sizeof(sifli_lv_res_mng_t)` rather than a naked magic 44.
- This is an **offline ABI compatibility improvement**, not evidence of OEM acceptance of third-party modules.

## 5. Remaining information that the public SDK does not supply

- The HK8 3407 **firmware binary image**, matching symbol table or corresponding `wf_obj_img_create` object file.
- Exact OEM `wf_obj_img_create` function prototype, implementation, pixel placement and image-memory/cache ownership.
- Target firmware's LVGL version, compiled feature flags, actual eZIP GPU driver and file-system path rules.
- Verification that firmware 2.09 can load arbitrary Monolith `.so`, plus safe rollback/recovery.
- Matching `monolith_res.so`, packaging and complete UI lifecycle; OEM face transfer does **not** prove custom module acceptance.

**Therefore the correct engineering status is: public manager/module-entry ABI substantially identified; generic SDK image loader implementation identified; proprietary `wf_obj_img_create` remains unresolved.** The BLE uploader is still disabled and no file has been sent to the physical watch.
