# PHASE 04 — HK8 3407 OEM eZIP rendering, placement and memory lifecycle (2026-10-23)

## Evidence and outcome

This report is grounded in **the private, captured** OEM `wf_clock23.so` and ten-file Wearfit Pro transfer, compared against the **unchanged existing** Monolith `clock_bg.bin`. Public repository commits contain only the analysis code, synthetic test fixtures and derived metadata. No OEM executable or Bluetooth capture has been uploaded to GitHub.

**Proven:** `wf_obj_img_create` is called with a resource descriptor; the module itself does not implement an eZIP decoder or issue a scaling function call. The concrete decoder, clipping behaviour, display placement, cache and image-memory ownership are delegated to firmware code that is **not present** in the recovered module. Thus the **actual** OEM eZIP decode, scale and memory strategies cannot be established from this binary alone.

## 1. Real image format and geometry

| Artifact | Source | LVGL image header | Width | Height | Type | Transferred bytes |
|---|---|---|---:|---:|---:|---:|
| Manufacturer background | `wf_clock23_bg.bin` | `0x3dc66802` | 410 | 494 | 2 | 28,148 |
| Manufacturer thumbnail | `wf_clock23_tn.bin` | `0x27841802` | 262 | 316 | 2 | 19,364 |
| Hour hand | `wf_clock23_h.bin` | `0x10c07805` | 30 | 134 | 5 | 12,068 |
| Minute hand | `wf_clock23_m.bin` | `0x1a407805` | 30 | 210 | 5 | 18,908 |
| Second hand | `wf_clock23_s.bin` | `0x1e803005` | 12 | 244 | 5 | 8,792 |
| Hour/minute centre | `wf_clock23_yuan_02.bin` | `0x03807002` | 28 | 28 | 2 | 1,072 |
| Second centre | `wf_clock23_yuan_01.bin` | `0x01803002` | 12 | 12 | 2 | 328 |
| **Existing Monolith** | `clock_bg.bin` | `0x41079401` | **485** | **520** | **1** | **158,937** |

All seven captured OEM image resources passed independent **CRC32/MPEG-2** footer verification; both the file sizes and image dimensions were read from actual captured bytes. Original Monolith BIN SHA-256: `e4d2412b8c2736fffe9b68f729d6cbbd1b1baefe4f68f2010c16bf60bfc03d9b`. The source must not be silently re-encoded.

A format discriminator number on its own does **not** guarantee the firmware accepts that encoding, decodes RGB correctly or respects its alpha channel.

## 2. What the stock Thumb code actually does

Exact CRC-stripped OEM ELF SHA-256:
`a037add2606e114e27705e2bdb0dd17e220fa32d2f1b712dfffd83697037bfec`.

`scripts/hk8_ezip_display_audit.py` checks this SHA, selected exact Thumb instructions, CRCs and the preserved Monolith image checksum. In the OEM event-0 branch:

- Context offset `0x24` supplies the parent LVGL object.
- `dlmodule_get_user_data(..., 28)` is called; a user-view object is initialized.
- `lv_cont_create` creates the root view, and `lv_obj_set_size` sets it to **410×494**.
- `wf_view_init` initializes the view.
- Subsequent `dlmodule_get_user_data(..., 28)` calls precede `wf_obj_img_create` (background) and `wf_obj_analog_clock_create` (hands).
- The stock image descriptor points to `/ex/resource/wf_clock23/wf_clock23_bg.bin`.

**No explicit image scaling/zoom or eZIP decode function is imported by this OEM module.** That is absence of a call *in the module*, not proof of no scaling/cropping inside firmware's `wf_obj_img_create`. The implementation of that OEM function was **not** available.

## 3. Placement — three mathematical options, none authorized as stock behaviour

Root 410×494 vs Monolith bitmap 485×520: horizontal excess **75 px**, vertical excess **26 px**.

| Hypothetical policy | Geometric output | Consequence |
|---|---|---|
| No scaling, centered crop | remove 37 left/38 right, 13 top/13 bottom | loses source image edges |
| Uniform contain | image 410×440 | approximately 54 px unused vertically |
| Uniform cover | image 461×494 | approximately 51 px cropped horizontally |

Those are **calculations, not observed watchface settings**. No transform was selected, no original design was changed, and the full graphic may be more constrained than expected if it contains marks near the edge.

## 4. Memory and view ownership — what can be established

The captured OEM `module_init` calls `rt_malloc(44)` to allocate a **44-byte resource manager** passed to `dlmodule_set_res_mng`. The view/event path calls `dlmodule_get_user_data(..., 28)`. In the event-3 cleanup path the OEM calls `wf_view_deinit`, then obtains/frees userdata when non-null. The separate `module_cleanup` obtains the resource manager, calls `lv_ext_res_mng_builtin_destroy` and then `rt_free` if the manager exists.

`watchface/prototypes/monolith_oem_lifetime_mock.*` models this ordering in an **independent, host-only mock**, rejecting duplicate opens, close-before-open, and resource-manager cleanup while a view remains active. It does **not** change the experimental ARM module and cannot certify the opaque firmware ownership semantics.

**Do not conflate these tiny 44/28-byte control objects with actual decompressed image memory.** Illustrative full-frame storage would be 756,600 bytes at RGB888 for the Monolith 485×520 image, or 810,160 bytes at hypothetical ARGB8888 for an OEM 410×494 buffer. The firmware may stream, tile, cache, decode in hardware or allocate differently — **none are demonstrated here**.

## 5. Available implementation and hard blockers

- Original Monolith source is present unchanged in `overlay/app/src/main/assets/monolith/clock_bg.bin`.
- Existing native prototype uses a *hypothetical* `wf_obj_img_create(user_data, descriptor)` call with a chosen, **unverified** `/ex/resource/monolith/clock_bg.bin` resource path. ABI and on-watch loading have **not** been verified; do not treat the ELF as installable.
- To implement actual rendering without guessing: obtain the matching firmware implementation/ABI for `wf_obj_img_create` and eZIP resource resolution, or collect independently verifiable controlled device evidence; establish accepted image type/pixel format, transform/clipping policy, cache and lifetime.
- Correct initialization and teardown for other OEM event selectors, actual scaling, anti-aliasing, LDC buffers, heap limits and safe recovery remain unresolved.
- The Android uploader remains fail-closed: `isUploadProtocolVerified()` is `false`.

## 6. Verification record

`python3 -m unittest discover -s scripts -p 'test_hk8_ezip_display_audit.py' -v` covers synthetic headers, CRC32 reference vector, incompatible geometry, rejected unknown ELF and an optional **private** OEM Thumb check. The archived OEM seven-image analysis was separately run against the actual captured originals and validated CRC in all seven.

The host-only C mock `monolith_oem_lifetime_mock.c` and `test_oem_lifetime_mock.c` compile with `cc -std=c11 -Wall -Wextra -Werror`; they confirmed the event order **G I D F M R** (get user, init view, deinit view, free user, destroy manager, free manager) and blocked resource cleanup while the view is open.

An offline test pass is **not** proof that the watch loads, scales or displays this custom resource. No physical watch or firmware was modified.
