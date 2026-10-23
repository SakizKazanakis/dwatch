# HK8 PRO MAX / HK ONE 3407 — Where `wf_obj_img_create` lives (2026-10-23)

**Evidence-level summary:** The actual implementation of `wf_obj_img_create` is **NOT PRESENT** in the transferred `wf_clock23.so` (it is imported as an undefined ELF symbol). A matching production firmware image for watch hardware code 3407 / reported version 2.09 has **NOT** been found or authenticated in searched public sources or the user-provided Wearfit Pro APK. Do not equate current public SiFli SDK code with the OEM ROM.

## Primary sources actually inspected

- Original private HK8 HCI-reconstructed `wf_clock23.so` and all ten genuine face files: stock ELF32 ARM EABI5 hard-float; undefined external `wf_obj_img_create` and `app_tileview_register_watchface`, confirmed via `readelf` and Thumb calls. OEM image `/ex/resource/wf_clock23/wf_clock23_bg.bin` is 410×494, type 2.
- [Public SiFli SDK module installer](https://github.com/OpenSiFli/SiFli-SDK/blob/main/middleware/include/mod_installer.h), [resource manager](https://github.com/OpenSiFli/SiFli-SDK/blob/main/middleware/lvgl/lv_ext_resouce/lv_ext_resource_manager_builtin.c), [RT-Thread dynamic loader](https://github.com/OpenSiFli/SiFli-SDK/blob/main/rtos/rtthread/components/libc/libdl/dlmodule.c): public implementation of module init, 44-byte resource-manager layout, 28-byte user data, allocation and release. The public SDK's `dlmodule_get_user_data` allocates once per module, then reuses that pointer.
- [Public SiFli image decoder](https://github.com/OpenSiFli/SiFli-SDK/blob/main/middleware/lvgl/lv_drivers/lvsf_img_decoder.c): file image loading, possible direct-mapped flash vs allocated image-cache/PSRAM and conditional free. **Not** established as the implementation of proprietary `wf_obj_img_create`.
- [Public SiFli eZIP example](https://github.com/OpenSiFli/SiFli-SDK/blob/main/example/multimedia/lvgl/ezip/src/main.c): `load_ezip`, `unload_ezip`, `lv_img_set_src` in either file mode or descriptor-in-memory mode.
- [Public SiFli example watchface](https://github.com/OpenSiFli/SiFli-SDK/blob/main/example/multimedia/lvgl/watch/src/gui_apps/clock/app_clock_dial.c): explicit image zoom through `lv_img_set_zoom`. This is evidence that public SDK supports a zoom route, **not** that OEM `wf_obj_img_create` applies it.
- [OpenEZIP](https://github.com/xeossys/OpenEZIP), independent experimental eZIP decoder and official converter wrapper (created 2026-06-22); project is research/WIP, not a proven firmware-matched HK8 loader. 
- [XDA / Wearfit ZIP/BIN discussion](https://xdaforums.com/t/w37-watch-7-cpu-hs6621.4362181/page-9): discusses ZIP dial packages containing SiFli eZIP BIN files, but not certified for HK8 board 3407.

Global GitHub public code-search for exact `wf_obj_img_create` and `app_tileview_register_watchface` returned **0 indexed matches** at research time. Search absence is not proof that no OEM firmware or private vendor SDK exists.

## Verified user-provided Wearfit Pro APK / OTA chain

Input: user-provided `com.wakeup.howear` APK (`hw_5.6.17`, code `2026101619`), not a firmware OTA package. Offline DEX class inspection located `com.wakeup.module.device.ota.SicheOtaMgr`. Its `startSiChe` method references `ZipUtils.UnZipFolder`, `AppPath.getAppOTACache` and these filename constants:

```text
outcom_ota.bin
outfont.bin
outlcpu.bin
outex.bin
outroot.bin
outlcpu_patch.bin
```

They show a real local OTA ZIP decompression and staged DFU path, **not** a verified URL or image for HK8 3407 firmware 2.09. Never conflate a vendor's other watch/firmware images with this target device. The supplied base APK is a split-only package without ABI `lib/` payloads or a cached device OTA. Need a matching and authenticated firmware archive, e.g. from manufacturer or authorized offline cache, before searching ROM exports or `wf_obj_img_create` machine code.

## Real eZIP corpus and disconfirmation of a naive raw-DEFLATE assumption

The recovered stock OEM background was 28,148 B (including trailing four-byte CRC32/MPEG2) at 410×494 / format 2. The existing Monolith `clock_bg.bin` is 158,937 B, 485×520 / format 1 and SHA256 `e4d2412b8c2736fffe9b68f729d6cbbd1b1baefe4f68f2010c16bf60bfc03d9b`.

OpenEZIP publicly documents raw-DEFLATE decoding plus experimental alternative legacy/shared-Huffman logic. In an independent offline reproduction, passing each real sample's bytes following the 4-byte LVGL header and 16-byte stream header directly to `zlib.decompress(data,-15)` failed with `invalid stored block lengths`. **The simplified raw-DEFLATE recipe does not decode these actual user assets.** This is not evidence of corrupt resource files; all ten genuine captured files previously passed the OEM CRC32/MPEG2 verification. We did not obtain a verified PNG round-trip against either exact sample. No claim of successful original-asset decompression is made.

## Firmware-matching blockers and recommended evidence route

1. Obtain an authenticated OEM OTA ZIP or firmware dump **specifically for HK ONE/HK8 board 3407, firmware 2.09**, without changing device state. Preserve the original bytes/manifest and verify matching model, hash, version and signature.
2. Locate the embedded root/HCPU image and firmware symbol table. Identify exported `wf_obj_img_create`, `wf_view_init`, `wf_obj_analog_clock_create`, `app_tileview_register_watchface`. Recover the actual function body and cross-references, then correlate with the four ARM arguments from the captured `wf_clock23.so`.
3. Establish exact firmware LVGL version, image decoder, video/GPU capabilities, memory allocation policy and file-system path mapping. Audit input image type 1/2 acceptance, view lifecycle and rollback without assuming `wf_obj_img_create` uses public SDK's file decoder implementation.
4. Independently test eZIP decoding and checksum against the real OEM/Monolith resources, compare with the known PNG, and classify encoding variants. An offline successful decoder is valuable but does not prove the OEM loader accepts a third-party `.so`.
5. No device writes until verified physical recovery/rollback and firmware version match. `SifliFilePush.isUploadProtocolVerified()` remains false.

**Exact present status:** public SDK resource-manager and dynamic loader code FOUND; public image decoder FOUND; private HK8 `wf_obj_img_create` implementation NOT FOUND; authentic target 3407 / 2.09 system ROM NOT FOUND; no new uploads/firmware writes.
