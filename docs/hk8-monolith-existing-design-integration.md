# Existing Monolith design — verified integration contract (HK8 3407)

Source of truth: branch `claude/project-thread-3o43p6`, `dial/monolith/clock_bg.png` and `clock_bg.bin`. Original PNG **485×520 RGB**, original eZIP `clock_bg.bin` **158937 bytes**. The Android asset `overlay/app/src/main/assets/monolith/clock_bg.bin` on the current watchface branch has **identical bytes** to the old source `dial/monolith/clock_bg.bin`; no image redesign, resampling or transcoding was performed.

Existing eZIP header bytes: `01 94 07 41` (`0x41079401`, width=485 height=520, format discriminator=1).

The verified OEM `wf_clock23.so` calls `wf_obj_img_create` with an image descriptor, and its original image resource lives under `/ex/resource/wf_clock23/`. This establishes a candidate integration API and path *pattern*, **not** acceptance of a Monolith file or of its eZIP board type.

The experimental integration declares a single explicit device resource path `/ex/resource/monolith/clock_bg.bin` for build-time validation. **This is a chosen hypothetical path, NOT a captured path or a verified upload destination.** It does not use `/dyn/` or claim a matching custom upload type. Actual file packaging and on-watch filesystem location are unresolved.

**Geometry conflict:** Monolith background 485×520 vs observed OEM root container 410×494. No automatic scaling is assumed. The planned rendering must choose a verified LVGL cropping or scaling method to preserve the image without distortion; offset and clip rules remain unknown. A dummy LVGL child container is **not** a decoded image.

**Current safety:** no firmware flashing, no executable upload, no modification of original design. The user-facing upload gate remains false. The only authorized test is offline parser/manifest inspection and host mocked callbacks.

## Offline LVGL module integration (implemented, hardware unverified)

- `monolith_design_resource.h/.c` declares the preserved resource path and the 3-field OEM-style image descriptor `[6,0,path]`. The tag values derive from `wf_clock23.so`; the new string path is a **hypothesis**. The file is not re-encoded or replaced.
- `test_design_resource.c` runs a fake renderer and checks the exact path, descriptor tag, zero flag and invalid arguments.
- `monolith_event0_module.c` now invokes the adapter on event `0` after creating the LVGL containers. It uses imported names `dlmodule_get_user_data` and `wf_obj_img_create` found in the OEM ELF, but the function-pointer C signatures are **not proven**, and the callback uses an inferred 28-byte user-data allocation.
- The native prototype links the descriptor helper and host tests in `build_event0_offline.sh`. All generated binaries remain offline-only; **do not upload**.
- The actual image path `/ex/resource/monolith/clock_bg.bin` is not known to exist on the watch; its packaging, image codec boardType, scaling/clipping, resource-manager mapping and display are not validated.
- The original 485×520 image exceeds the observed 410×494 root dimensions by **75px horizontally and 26px vertically**. The integration deliberately makes no unverified transformation.

**Success criterion for this stage:** building and statically checking an ELF that references the original Monolith design via the OEM-style image call with host-tested resource metadata. This does **not** establish a working watchface or safe physical installation.
