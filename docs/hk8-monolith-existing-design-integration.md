# Existing Monolith design — verified integration contract (HK8 3407)

Source of truth: branch `claude/project-thread-3o43p6`, `dial/monolith/clock_bg.png` and `clock_bg.bin`. Original PNG **485×520 RGB**, original eZIP `clock_bg.bin` **158937 bytes**. The Android asset `overlay/app/src/main/assets/monolith/clock_bg.bin` on the current watchface branch has **identical bytes** to the old source `dial/monolith/clock_bg.bin`; no image redesign, resampling or transcoding was performed.

Existing eZIP header bytes: `01 94 07 41` (`0x41079401`, width=485 height=520, format discriminator=1).

The verified OEM `wf_clock23.so` calls `wf_obj_img_create` with an image descriptor, and its original image resource lives under `/ex/resource/wf_clock23/`. This establishes a candidate integration API and path *pattern*, **not** acceptance of a Monolith file or of its eZIP board type.

The experimental integration declares a single explicit device resource path `/ex/resource/monolith/clock_bg.bin` for build-time validation. **This is a chosen hypothetical path, NOT a captured path or a verified upload destination.** It does not use `/dyn/` or claim a matching custom upload type. Actual file packaging and on-watch filesystem location are unresolved.

**Geometry conflict:** Monolith background 485×520 vs observed OEM root container 410×494. No automatic scaling is assumed. The planned rendering must choose a verified LVGL cropping or scaling method to preserve the image without distortion; offset and clip rules remain unknown. A dummy LVGL child container is **not** a decoded image.

**Current safety:** no firmware flashing, no executable upload, no modification of original design. The user-facing upload gate remains false. The only authorized test is offline parser/manifest inspection and host mocked callbacks.
