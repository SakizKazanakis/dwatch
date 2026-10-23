# PHASE 04.2 — OEM `wf_clock23` registration reconstructed from ARM Thumb instructions

**Evidence:** 3,412-byte CRC-stripped `wf_clock23.so` reconstructed from the owner's recorded, successful Wearfit Pro stock-watchface transfer. SHA-256:
`a037add2606e114e27705e2bdb0dd17e220fa32d2f1b712dfffd83697037bfec`.

**Scope:** one OEM binary, HK8 PRO MAX / HK ONE code 3407, system 2.09. Verified by actual ARM Thumb disassembly, ELF32 symbol and relocation inspection, and exact PC-relative literal / GOT calculations. **This is NOT an authorized or tested third-party extension interface.** No OEM binaries, private logs, user identifiers, or OTA images are committed.

## 1. Registration call: four observed AAPCS argument registers

`wf_clock23_register`: Thumb entry `0x7d5`, 56-byte function (entry instruction address `0x7d4`).

At `0x7d6`–`0x7f4`, the thunk loads an address from GOT slot `0xab4`, resolves it to imported `app_tileview_register_watchface` via relocation `R_ARM_GLOB_DAT`, sets up registers, and tail-branches with `bx r12`.

| ARM register | Computed runtime value | Evidence |
|---|---|---|
| `R0` | pointer to `"wf_clock23"` | PC-relative string at `0x8b4` |
| `R1` | `0` | instruction `movs r1,#0` |
| `R2` | pointer to `"/ex/installer_wf/wf_clock23_tn.bin"` | PC-relative string at `0x9c2` |
| `R3` | Thumb function pointer `0x669` | PC-relative signed literal `0xfffffe7d`, target code `0x668` |
| Branch target | `app_tileview_register_watchface` | GOT `0xab4`, relocation + `bx r12` |

Exact thumb arithmetic tested:

```text
GOT base:       0x7da + 4 + 0x2a2      = 0xa80
Call target:    GOT[0xa80+0x34]         = app_tileview_register_watchface
R0 string:      0x7ec + 4 + 0xc4        = 0x8b4
R2 thumbnail:   0x7ea + 4 + 0x1d4       = 0x9c2
R3 callback:    0x7e8 + 4 - 0x183       = 0x669  (Thumb entry)
```

This **does not establish the source-level C prototype** or the meaning of argument `R1=0`. It establishes only the machine-level argument values actually supplied by this OEM binary. No valid fifth argument has been established.

## 2. Loader-facing `module_init`: observed call sequence

`module_init`: Thumb entry `0x80d`, 108 bytes. It preserves incoming `R0` (an opaque module argument) before performing:

| Order | Imported or local function | Observed arguments / effect |
|---:|---|---|
| 1 | `rt_malloc` | `R0=0x2c`: allocate **44 bytes** |
| 2 | `dlmodule_set_res_mng` | `R0=opaque original module`, `R1=allocated pointer` |
| 3 | `lv_ext_res_mng_builtin_init` | `R0=allocated pointer`, `R1=lv_i18n_lang_pack` imported address |
| 4 | `lv_ext_get_locale` | Returned value used as locale |
| 5 | `lv_ext_set_locale` | `R0=allocated pointer`, `R1=returned locale` |
| 6 | `wf_clock23_register` | Tail call to the above registration thunk |

This is a decompiled **flow description**, not a published C header. Opaque structures and return codes are unknown; the binary contains no observable safe-loader acceptance or verification of a third-party resource pack.

## 3. Callback and view lifecycle

The `R3` callback at `0x668` takes an event selector in `R0` and a pointer-like context in `R1`. A Thumb `TBB [pc,r0]` at `0x678` dispatches four entries from the jump table at `0x67c`:

| Selector | Branch offset | Observed calls / behavior |
|---:|---|---|
| 0 | `0x680` | Create LVGL container, size and alignment; allocate/access user-data, initialise view and graphic resources |
| 1 | `0x720` | `app_tileview_only_change_anim_status(context,1)`; `wf_view_resume` on the observed zero-result branch |
| 2 | `0x742` | `app_tileview_only_change_anim_status(context,2)`; `wf_view_pause` on the observed zero-result branch |
| 3 | `0x760` | `wf_view_deinit`, `dlmodule_free_user_data` (conditional on returned user data) |

In selector 0, code loads `[context+0x24]`, then calls `lv_cont_create(parent,0)`, `lv_obj_set_size(container,410,494)`, `lv_obj_align`, `lv_obj_set_parent_event`, `lv_page_glue_obj`, `wf_view_init`, `wf_obj_img_create`, and `wf_obj_analog_clock_create`. The function requests a **28-byte user-data object** via `dlmodule_get_user_data("wf_clock23",28)`. Two 12-byte static descriptors are copied from `.data` onto the stack and passed to resource creation calls. **Their C structure layout, individual fields and ownership semantics remain unverified.**

### Static resource records passed to the UI creation calls

The 12-byte records copied onto the stack by the event-0 handler were independently checked against the **correct ELF section-to-file mapping** (the `.data` section has a different file offset from `.text`). Values below are the raw little-endian 32-bit words, before runtime relocations:

| Virtual record | Word 0 | Word 1 | Word 2 |
|---|---|---|---|
| `0xaf0` | `0x00000006` | `0` | `0x000008bf` |
| `0xafc` | `0x00cd0005` | `0x000000f7` | `0x000009ec` |

Relocated `0x8bf` points to `/ex/resource/wf_clock23/wf_clock23_bg.bin`. Relocated `0x9ec` points to a separate 36-byte table in `.data.rel.ro`, containing raw words and references to `wf_clock23_h.bin`, `wf_clock23_yuan_02.bin`, `wf_clock23_m.bin`, `wf_clock23_s.bin` and `wf_clock23_yuan_01.bin`.

**The record field names, pixel/position encoding and the actual C struct type cannot yet be asserted.** We know the exact pointers and their referenced names, not their proprietary semantics.

### Destroying the module

`module_cleanup`: Thumb entry `0x879`, 60 bytes. It calls `dlmodule_get_res_mng` on the incoming module argument; when the returned manager is non-null, it calls `lv_ext_res_mng_builtin_destroy(manager)` followed by `rt_free(manager)`. This cleanup logic is separate from the selector-3 view teardown.

## 4. Reproducibility

A **read-only, SHA-256-pinned** extractor is provided at
`scripts/hk8_watchface_registration_probe.py`. It parses ELF sections,
`R_ARM_GLOB_DAT` relocations and fixed-address Thumb literal targets; it
will refuse any file except the verified captured specimen.

Private hardware evidence can be checked offline:

```sh
MONOLITH_OEM_FACE_ELF=/path/to/crc-stripped/wf_clock23.so \
  python3 -m unittest discover -s scripts -p "test_hk8_registration_abi.py" -v
python3 scripts/hk8_watchface_registration_probe.py \
  /path/to/crc-stripped/wf_clock23.so
```

Public CI runs arithmetic/structural checks but **skips** the sample-dependent test: proprietary device binaries are never included in this repository. Locally, the original specimen passed all seven probe tests.

## 5. Offline native registration experiment — with a linker incompatibility detected and contained

The repo also includes `watchface/prototypes/monolith_registration_hypothesis.c`, which models the observed `module_init` → `app_tileview_register_watchface` lifecycle with a **no-op callback**. Its `extern` C function-pointer signatures are **explicit hypotheses**, *not* verified OEM SDK declarations. Nothing from this model is called from the Android uploader.

A first conventional Clang/LLD build generated `R_ARM_JUMP_SLOT` relocations, **unlike the captured OEM**, which uses `R_ARM_GLOB_DAT` and `R_ARM_RELATIVE`. Compiling a superficially matching ELF with the default PLT is therefore insufficient.

The experiment now loads external imported addresses through indirect GOT references, using a compiler barrier in `RESOLVE()`. The resulting offline module is **ARM Thumb, ELF32, ET_DYN, ARM EABI5 hard-float** and its external symbol references are `R_ARM_GLOB_DAT`; CI explicitly rejects `R_ARM_JUMP_SLOT` and `R_ARM_COPY`. This matches *some* observed relocation classes and argument register placement, **not** the complete OEM relocation layout or supported loader semantics.

To compile *locally without touching the watch*:

```sh
bash watchface/prototypes/build_registration_hypothesis.sh /tmp/monolith_registration_hypothesis.so
```

Do not treat this as a functional dial: `monolith_event_callback` does not draw a UI; its prototype, resource pack, `.dsc` and context structures are not independently validated. No such module is attached to a release or included in the APK.

## 6. What still blocks a working Monolith module

1. Precise types and semantics of the registration API, the callback context object, resources and locale pack are **not** recovered from a public header.
2. The observed `R3` callback ABI shows event and context registers; its exact source-level return type, thread affinity and lifetime guarantees are unknown.
3. The OEM `wf_clock23.so` being transferred successfully does not prove that third-party ARM modules will link or load.
4. The original interface may depend on proprietary LVGL/RT-Thread builds and firmware-specific exported symbols.
5. There is **no verified safe rollback/recovery** for arbitrary file or module writes to this HK8 revision.

**Shipping policy:** uploader remains disabled. No binary from this analysis is installable on the watch. Next research must validate full context layout, descriptor conventions, memory ownership and module-loader checks before a controlled reversible test.
