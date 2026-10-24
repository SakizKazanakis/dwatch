# MONOLITH PHASE 05–07 — execution checkpoint 2026-10-24

## Verified real-source results (not from generated mock fixtures)

- Input: existing `overlay/app/src/main/assets/monolith/clock_bg.bin`, 158,937 bytes, SHA-256 `e4d2412b8c2736fffe9b68f729d6cbbd1b1baefe4f68f2010c16bf60bfc03d9b`.
- Successful decompression of **actual source BIN** with *legacy shared-Huffman* eZIP, including bitstream canonical Huffman tables, 4-byte aligned blocks, DEFLATE references, per-row PNG filters with 32-row resets, and BGR888 → RGB888 transformation.
- Expanded filtered bytes **757,120**, exact final RGB bytes **756,600** (=485×520×3).
- Final RGB SHA-256: `1b02602419be4dd7c8e9db64073df99621d6542fa0c1eb5bfec685d46d77a940`.
- Compared directly against the original source `dial/monolith/clock_bg.png` from the historical branch: 252,200/252,200 pixels exact equal; channel MAE **0.0**, maximum absolute error **0**, mismatching pixels **0**. This is a genuine decoder pass; the PNG is not used in the decode pipeline.
- Re-encoded a valid standalone PNG directly from the decoded data using standard-library zlib. The PNG roundtrip recovers the same RGB bytes.
- Five Python decoder checks passed, including original PNG comparison when the separate historical PNG is supplied, lossless encoded PNG roundtrip, invalid inputs and decompression bounds.
- An ARM32 C native **RGB888 framebuffer compositor**, plus working host CMake target and test, has been created. It borrows immutable decoded RGB data, draws digital HH:MM on top, exposes init/draw/pause/resume/deinit and tracks its owned 756,600-byte framebuffer. 1,000 host lifecycle cycles passed, including repeated deinit, pause/resume and invalid times; real output frame generated to a PPM file.
- The native C file cross-compiled to **ELF32 ARM relocatable object**, ARM EABI5, using Clang with freestanding declarations. That is **not** a dynamically loadable watchface module.
- All experiment binaries and custom rendering assets are offline only; no BLE file writes and no firmware modifications.

## Actual implementation paths

- `watchface/ezip/monolith_decoder.py`: pure-Python eZIP legacy shared-Huffman real source decoder, robust header & source hash validation, stream bounds, filters, RGB conversion and genuine PNG writer.
- `tests/test_monolith_ezip_decode.py`: source file, original PNG equality, roundtrip and corruption tests.
- `watchface/native_renderer/include/monolith_renderer.h`, `src/monolith_renderer.c`, `tests/test_renderer.c`, `CMakeLists.txt`: real portable C framebuffer rendering, 1,000-cycle C test, output PPM.
- `.github/workflows/build.yml`: Python full-source decode, native host tests and ARM ELF32 relocatable compilation.

## Build / reproduction commands

```bash
python3 watchface/ezip/monolith_decoder.py \
  overlay/app/src/main/assets/monolith/clock_bg.bin \
  --png /tmp/monolith_decoded.png \
  --raw /tmp/monolith_decoded.rgb

MONOLITH_ORIGINAL_PNG=/path/to/original/dial/monolith/clock_bg.png \
  python3 -m unittest discover -s tests -p 'test_monolith_ezip_decode.py' -v

cmake -S watchface/native_renderer -B /tmp/monolith-native -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/monolith-native
ctest --test-dir /tmp/monolith-native --output-on-failure
/tmp/monolith-native/monolith_native_renderer_test \
  /tmp/monolith_decoded.rgb /tmp/monolith_frame.ppm
```

## Original provisional limitations (superseded where noted below)

1. This new C framebuffer compositor is **not an actual LVGL renderer** and has no native integration into the actual HK8 firmware yet.
2. **SUPERSEDED:** Native C legacy shared-Huffman decoder is now implemented and passed byte-for-byte verification against the Python decoder.
3. Digital time overlay works on host; actual device time/date RTC access, temperature, sensors, background scheduling, locale or LVGL timers are not wired.
4. Physical HK8 3407 / version 2.09 loader and `wf_obj_img_create` compatibility are not proven. ARM `ET_REL` does **not** imply support for OEM `ET_DYN` installable modules.
5. Native memory on the device is unknown; storing an RGB888 background plus a full RGB888 framebuffer would consume **1,513,200 bytes**, without decoding buffers or UI objects. This may exceed available RAM. It is not an accepted on-device architecture.
6. No real-device rollback/restore mechanism has been verified. `SifliFilePush.isUploadProtocolVerified()` remains disabled.
7. Full CI for the latest commit must be checked separately; do not generalize local host passes into CI PASS.

## Next gates in strict order

- Implement and test **C native streaming/shared-Huffman decoder** for original eZIP and validate pixel SHA against Python reference.
- Reduce peak memory by scanline/blit into verified frame-buffer API; validate 485×520 panel geometry, clipping and actual LVGL memory limits.
- Use firmware-version-matched native LVGL and RT-Thread headers to build a true LVGL object tree, dynamic date/time and safe events 0–3.
- Build OEM-compatible ET_DYN if and only if the true external registration APIs and symbol resolution are verified.
- Do not attempt hardware loading until authorized recovery path and explicit physical trial approval.

Status: **PHASE 05.1 PASS**, **PHASE 05.2/05.3 PARTIAL**, **PHASE 06 host graphics PARTIAL**, **PHASE 07 hardware BLOCKED**.

## Updated verified PHASE 05.3 — native C eZIP and real LVGL8 renderer

- `src/monolith_ezip.c`: the original 158,937-byte Monolith file was decoded **directly in C** to 756,600 RGB bytes. SHA256 `1b02602419be4dd7c8e9db64073df99621d6542fa0c1eb5bfec685d46d77a940`, exact match with the independent Python decoder and original PNG. The former unsupported decode stub has been removed.
- Both native renderer and native decoder compiled to ARM32 ELF relocatable objects (EABI5). **Not** an OEM-compatible installable ET_DYN.
- `monolith_lvgl8_adapter.c` now implements actual public LVGL v8.3 image, time/date labels, injectable one-second clock source, pause/resume and deletion including timer.
- Host build against official LVGL 8.3 git revision `fbb73d4b3ffe086a7218bd1a8e51f9e7a260c4b9`: rendered original design through a real LVGL flush callback, **104 flushes**, time change produced a different frame, test passed. Source remains unchanged.
- Device compatibility unknown: public LVGL8 does not establish OEM firmware version/API compatibility. `wf_obj_img_create` and custom loader acceptance remain unknown.
- Memory risk: temporary eZIP 757,120 bytes + target decoded RGB 756,600 bytes; public LVGL8 32-bit true-colour image adds 1,008,800 bytes. These allocations are **not validated** for physical HK8 RAM/PSRAM.
- The C implementation currently handles the verified original legacy-Huffman format, not arbitrary eZIP variants. Physical recovery and on-watch write mechanisms are not proven. **No device writes**.

