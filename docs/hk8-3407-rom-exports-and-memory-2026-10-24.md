# HK8 Pro Max 3407 — ELF import dependency and RAM/PSRAM evidence audit
**Date:** 2026-10-24 · **Read-only**, no watch writes · branch `research/hk8-3407-rom-symbols-memory-20261024`

## Verified exact transferred OEM module

Original file extracted from the user's BLE/HCI reconstruction: `ex/installer_wf/wf_clock23.so`. ELF32 ARM EABI5 ET_DYN, **3416 bytes**. Exported functions: `wf_clock23_register`, `module_init`, `module_cleanup`.

The original ELF's `.dynsym` contains **24 external named imports**, all observed directly with `readelf -Ws`; `R_ARM_GLOB_DAT` entries appear in `.rel.dyn`.

| Group | Exact names | Count |
|---|---|---:|
| LVGL / LVGL extensions | `lv_obj_align`, `lv_cont_create`, `lv_ext_res_mng_builtin_destroy`, `lv_ext_res_mng_builtin_init`, `lv_ext_get_locale`, `lv_obj_set_parent_event`, `lv_obj_set_size`, `lv_page_glue_obj`, `lv_ext_set_locale` | 9 |
| RT-Thread / module loader | `rt_free`, `dlmodule_free_user_data`, `rt_malloc`, `dlmodule_set_res_mng`, `dlmodule_get_user_data`, `dlmodule_get_res_mng` | 6 |
| Manufacturer watchface UI | `wf_obj_analog_clock_create`, `wf_view_init`, `wf_view_deinit`, `app_tileview_only_change_anim_status`, `app_tileview_register_watchface`, `wf_obj_img_create`, `wf_view_pause`, `wf_view_resume` | 8 |
| Other module resource | `lv_i18n_lang_pack` | 1 |

**Important:** These 24 names are requirements of one stock watchface executable. They are **not** the complete exported symbol table of watch firmware. It is not established that every import comes directly from the kernel/ROM: an external module may provide it.

The separate transferred `wf_clock23_res.so` exports `lv_i18n_lang_pack` (108-byte object), so that requirement can be satisfied by a *separate resource module*, not necessarily ROM.

## How symbol resolution actually works

The public [SiFli-SDK `dlmodule.c`](https://github.com/OpenSiFli/SiFli-SDK/blob/main/rtos/rtthread/components/libc/libdl/dlmodule.c) function `dlmodule_symbol_find` first searches kernel `_rt_module_symtab_begin → _rt_module_symtab_end`; it may then invoke a separately registered `dlmodule_ex_sym_resolver`. The same source exports `dlmodule_get_user_data`, `dlmodule_free_user_data`, `dlmodule_get_res_mng`, `dlmodule_set_res_mng` through `RTM_EXPORT`.

The presence of an `RTM_EXPORT` macro in a **public** SDK proves the public API's intent, **not that the binary installed on the specific HK8 3407 exported it**. Even knowing the processor family does not reveal the device's runtime symbol table, linker script or vendor functions.

## Memory budget — physical capacity vs actual free heap

The public SF32LB55x datasheet and an independently published HK8 Pro Max commercial product specification point to **SiFli SF32LB551** and display **410×494** as a plausible match. The observed OEM eZIP image itself is 410×494. However the exact SoC marking and installed memory chips of the **user's Code 3407 revision have not been directly measured or visually verified**.

For the SF32LB55x family, public documentation lists HCPU SRAM **1,088 KiB** and LCPU SRAM **224 KiB** (these are distinct address/bank allocations, not a single allocatable heap), and an interface for external PSRAM. Its fitted capacity is board/BOM dependent.

| Buffer / requirement | Bytes | Evidence |
|---|---:|---|
| Stock 410×494 RGB565 framebuffer | 405080 | Pure arithmetic for hypothetical RGB565 |
| Stock 410×494 32-bit framebuffer | 810160 | Pure arithmetic for hypothetical 32-bit pixels |
| Original Monolith 485×520 decoded RGB888 | 756600 | Real eZIP decode |
| Original Monolith 485×520 LVGL 16-bit image | 504400 | Colour-depth calculation |
| Original Monolith 485×520 LVGL 32-bit image | 1008800 | Host LVGL test setup |
| Verified stream decoder scratch | 37133 | Implemented C code |
| Monolith 32-bit image + scratch | 1045933 | Arithmetic only |
| SF32LB55x family HCPU SRAM | 1114112 | Family spec, **not free heap** |
| SF32LB55x family LCPU SRAM | 229376 | Family spec, **not free heap** |

The 32-bit Monolith image (1,008,800 bytes) plus stream scratch (37,133 bytes) totals **1,045,933 bytes**, close to the **1,114,112-byte family HCPU SRAM** before kernel, stack, LVGL heap, display buffers and other allocations. Thus this scheme is *unlikely* to fit entirely in internal HCPU SRAM if the device's implementation follows that architecture. This is an engineering constraint, **not** a measured OOM or proof that PSRAM is absent. Hardware eZIP, PSRAM or a lower-colour-depth scanline path could change the result.

**Actual Code 3407 values:**
- RT-Thread total/free heap: **NOT MEASURED**.
- Largest contiguous allocatable block: **NOT MEASURED**.
- Installed and available PSRAM: **NOT MEASURED**.
- Memory partition and allocation classes of `lv_mem_alloc`, `rt_malloc`, `app_cache_alloc`: **NOT MEASURED**.
- SRAM allocated by watchface runtime when a stock face is active: **NOT MEASURED**.

Android `/proc/meminfo` from the phone's bugreport would describe the **phone**, not the smartwatch. Do not use it as watch telemetry.

## Implemented reproducible tools

- `scripts/hk8_firmware_exports_and_ram.py` reads ELF dynamic symbol tables with `readelf --dyn-syms`, records exact imports and exports, and compares external providers if an authenticated candidate ROM ELF is provided. It deliberately never labels a candidate as target firmware without board/version authentication.
- Passing the stock `wf_clock23.so` and `wf_clock23_res.so` classifies `lv_i18n_lang_pack` as an import with a **candidate provider in the separate resource ELF**.
- It optionally parses saved genuine RT-Thread `msh> free` output (total, used, high-water) and explicitly does *not* infer PSRAM or largest free block.
- Three unit tests check arithmetic, measured-log parsing limitations, and a real synthetic ARM ELF with unresolved dynamic imports.
- GitHub CI executes those tests without bundling the private OEM ELF.

Example (offline; filenames refer to user-provided private local data, **never commit**):

```bash
python3 scripts/hk8_firmware_exports_and_ram.py \
  --face-elf /private/ex/installer_wf/wf_clock23.so \
  --resource-elf /private/ex/installer_wf/wf_clock23_res.so \
  --output /private/hk8_symbols_and_memory.json
```

## Hard next evidence gate

To reconstruct **the exact complete 3407 firmware export set**, obtain a firmware-2.09 authenticated system ELF with `.rtmsymtab`, a vendor link map, or a read-only runtime symbol-table dump. The transferred watchface module does not contain the entire ROM export table. For a real free-memory profile, obtain **device-executed** diagnostic metrics via an already documented read-only interface or a vendor system diagnostic. Public standard BLE information and the OEM watchface transfer alone do **not** expose RTOS heap internals.

No firmware OTA, ELF injection, BLE write, shell command or flash read has been sent to any watch in this audit.

**Status:** EXACT STOCK FACE IMPORTS = PASS; FULL ROM EXPORTS = BLOCKED; ACTUAL FREE RAM/PSRAM = BLOCKED; STATIC BUFFER SIZING = PASS; HARDWARE LOAD = NOT TESTED.
