# HK8 PRO MAX / HK ONE 3407 — PHASE 04: LVGL resources and event-0 prototype

Evidence cut: 2026-10-23. This is an **offline-only engineering milestone**, **not a runnable watchface on stock firmware**.

## 1. Source and confidence

The OEM baseline is the owner's privately recovered `wf_clock23.so`, 3412 bytes **without the 4-byte CRC**, SHA-256 `a037add2606e114e27705e2bdb0dd17e220fa32d2f1b712dfffd83697037bfec`. A real Wearfit Pro transaction successfully sent ten `wf_clock23` resources. The upstream ELF, HCI capture, OEM images and other personal material are **not** committed.

The firmware emits the following symbol *names*, and the OEM Thumb code calls them from its event-0 branch: `lv_cont_create`, `lv_obj_set_size`, `lv_obj_align`, `lv_obj_set_parent_event`, `lv_page_glue_obj`, `wf_view_init`, `wf_obj_img_create`, `wf_obj_analog_clock_create`. This is direct original binary evidence, **not** a verified set of OEM C header prototypes. LVGL v7 publicly documents the two-argument `lv_cont_create(parent,copy)` family (https://lvgl.io/docs/open/7.11/overview/object); the local firmware may use a vendor fork or a different ABI.

## 2. Precisely recovered resource records

`scripts/hk8_lvgl_resource_inspector.py` pins that exact ELF SHA, checks every relevant `R_ARM_RELATIVE` relocation and decodes only **raw little-endian 32-bit words plus resolved string pointers**. It rejects any other file.

| Virtual address | Word 0 | Word 1 | Word 2 | OEM call site |
|---|---|---|---|---|
| `0xAF0` | `0x00000006` | `0x00000000` | `0x000008BF` | Passed to `wf_obj_img_create(user_data,&record)` |
| `0xAFC` | `0x00CD0005` | `0x000000F7` | `0x000009EC` | Passed to `wf_obj_analog_clock_create(user_data,&record)` |

The first record points to `/ex/resource/wf_clock23/wf_clock23_bg.bin`. The second record points to a separate 36-byte table at `0x9EC` consisting of three triples:

| Triplet | Resource path 1 | Resource path 2 | Raw packed word |
|---|---|---|---|
| 1 | `wf_clock23_h.bin` | `wf_clock23_yuan_02.bin` | `0x007E000F` |
| 2 | `wf_clock23_m.bin` | `wf_clock23_yuan_02.bin` | `0x00CA000F` |
| 3 | `wf_clock23_s.bin` | `wf_clock23_yuan_01.bin` | `0x00CA0006` |

Every listed pointer has the expected `R_ARM_RELATIVE` relocation. **Opaque field meanings remain UNKNOWN**: treating `0xCD0005` as pixel coordinates, image type, animation flag or dimensions would be speculation. No `typedef` for OEM `wf_obj_img_create` descriptors is claimed.

## 3. Working *offline* event-0 creation logic

`watchface/prototypes/monolith_event0.c` implements a host-tested event dispatcher:

- Selector `0`: read parent pointer at callback context byte offset `0x24` (**OEM Thumb evidence for a 32-bit ARM context**, not a generalized public structure).
- Create root via `lv_cont_create(parent, NULL)`; size `410 × 494`; align to parent at `(0,0)`; call parent-event and page-glue hooks with `1` (matches OEM call sequence).
- Create one **independent Monolith graphic container** as child of root; size `164 × 64`; position `(24,24)`. These latter dimensions and position are a **Monolith design choice**.
- Reject null callbacks, null parent and failed object creation.
- Other lifecycle selectors return without runtime actions: OEM pause/resume/destroy ownership semantics have not been implemented yet; this is an explicitly incomplete prototype.

`test_event0.c` executes the **same event-0 implementation** against fake LVGL C function pointers on the host. The test asserts the exact creation calls, parent relations, 410×494 and 164×64 sizes, alignment values, page glue, parent-event flag and null handling. This validates the C event handler's control flow, **not** its actual display output on HK8.

### ARM ELF experimental wrapper

`monolith_event0_module.c` uses the recovered four-register `app_tileview_register_watchface` call pattern. Its C type aliases are **hypotheses**, not verified firmware headers. It includes a callback that dispatches event `0` into `monolith_create_view()`. `build_event0_offline.sh` runs the host simulator and builds an ARM Thumb ELF32 `ET_DYN` module using Clang/LLD, EABI5 hard-float.

The native module uses only external import names observed in the OEM `wf_clock23.so`; `scripts/check_hk8_arm_module.py` checks the header, SysV hash, symbols and `R_ARM_GLOB_DAT` relocation classes. **This is a static similarity check, not an OEM loader test.**

**Important unresolved display issue:** LVGL default styles in this firmware are not yet inspected. The C code creates real LVGL object *calls*, but colour, appearance and visibility of the child panel on the physical watch are not proven. Functions for explicit styling/text/images are deliberately *not* imported because they have not been confirmed to exist on this firmware. Therefore this is **not** yet a verified visually functioning HK8 watchface.

## 4. Independent checks / limits

| Gate | Test | Interpretation |
|---|---|---|
| OEM descriptor read | SHA-pinned Python analyzer + synthetic/public tests + owner-only exact-ELF test | Exact words, pointer links, ARM relocation types |
| Event creation | Host C mock compiled with `-Wall -Wextra -Werror` | Program constructs correctly shaped two-object tree |
| Native ABI | ARM32 linker + `readelf` symbol/relocation checks | Basic ELF/ABI and imported symbol-name compatibility |
| Runtime symbol resolution | No live firmware symbol table available | **UNKNOWN** |
| Native loader acceptance | No safe physical experiment performed | **UNKNOWN** |
| Real graphical output | No stock LVGL runtime/emulator for this ROM | **NOT TESTED** |
| Original custom resource encoding | OEM `.bin` bytes available privately; no verified encoder for the exact path | **NOT VERIFIED** |
| Lifecycle destroy/re-init safety | Host-only event 0; no real teardown | **NOT READY** |
| BLE no-response flow control | Not exercised | **NOT VERIFIED** |
| Recovery/rollback | No proven rescue firmware/tool for physical unit | **NOT READY** |

In particular, the demonstration binary refers to `lv_i18n_lang_pack`, which the OEM stock `wf_clock23_res.so` exports; **we do not have a matching Monolith resource pack**. The absence of an authoritative firmware symbol export list means even identical undefined-symbol names cannot guarantee they will resolve.

## 5. Safe physical test plan — preconditions, not permission to write

1. Confirm exact hardware revision, firmware build/partition configuration and supported third-party module mechanism. Code 3407 and GATT identifiers alone are not enough.
2. Recover trusted public or OEM source-level headers, especially the app registration prototype, LVGL event context, resource manager, view teardown and all image descriptors; verify with the OEM Thumb call-site evidence.
3. Implement event selectors 1/2/3 with verified resource ownership and full release behavior; test repeat install/open/close in an emulator or verified matching hardware.
4. Generate a complete compatible resource pack and `.dsc` using measured formats, CRCs and paths, not guesses. Confirm that all required imports resolve against firmware-provided symbols and packaged resource modules.
5. Prove a hardware-specific backup/rollback and recovery route, including power-loss during installation, *before* uploading native executables.
6. Only after all gates pass, obtain explicit approval for a **single reversible test** with recorded pre/post state, monitoring and abort criteria. Never experiment with arbitrary OTA firmware writes.

**Decision today:** `SifliFilePush.isUploadProtocolVerified()` remains `false`, UI dial upload remains disabled, no generated `.so` is deployed or distributed as an installable watchface, and nothing has been written to the physical HK8.
