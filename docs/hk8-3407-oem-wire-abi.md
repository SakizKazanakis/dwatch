# HK8 PRO MAX / 3407 — OEM type-0 wire codec and ARM ABI (PHASE 03.5)

Date: 2026-10-23. Scope: **offline validation only**. Based on a successful OEM `wf_clock23` HCI transaction and a user-provided recovered stock watchface archive. Neither raw trace nor OEM executable binaries are committed to GitHub.

## Source-of-truth boundaries

- On-watch observations: HK ONE / HK8 PRO MAX, code 3407, system 2.09; SiFli 0x0200 and Wearfit 0x0002 characteristics present.
- HCI verified: one official watchface installation via Wearfit Pro with 10 files, 97,448 bytes, 29 data blocks, and valid positive transfer acknowledgements.
- Offline source cross-check: `SifliWireCodecJvmTest` compares the captured START request and START ACK, builds `FILE_START`, encodes 4096-byte data blocks, checks 244-byte fragments, and tests CRC/overflow/malformed frames.
- **NOT verified:** upload of a custom background (type 3); accepting a third-party `.so`; actual display behavior; actual method signatures of RT-Thread/LVGL; bootloader or flash modification; hardware recovery procedure.

## Exact OEM wire facts

The captured SiFli characteristic value for transfer START is (hex):

```text
04 00 0b 00 00 00 07 00 00 00 02 a8 7c 01 00
```

Outer `04 00 0b 00` = SiFli category 4, single-fragment flag 0, 11-byte payload.
Inner `00 00 07 00 00 00 02 a8 7c 01 00` = START 0, inner length 7, type 0, flag 2, total `0x00017ca8=97448` bytes.

Observed START reply (complete notification) is:

```text
04 00 06 00 01 00 00 00 00 28
```

That means response opcode 1 and result 0. The final `00 28` bytes are **not interpreted** until verified by the OEM source.

First file header excerpt observed in the HCI capture:

```text
04 00 33 00 02 00 2f 00 f4 6d 00 00 29 00 ...
```

This encodes FILE_START=2, body length=47, transmitted size=28,148 and 41-byte path. The remote path is `/ex/resource/wf_clock23/wf_clock23_bg.bin`. A file-start message is **not** the same operation as a firmware flash command.

The OEM session used 4,096-byte logical file-data blocks, broken into serial-transport characteristic values of no more than 244 bytes. ATT MTU negotiated to 517, which **does not** imply 514-byte SiFli packet payloads. Captured command/ACK codes: 0→1, 2→3, 4→5, 6→7, 8→9. All ten payloads included a little-endian CRC32/MPEG-2 footer matching the preceding bytes; no CRC is appended to type-0 OEM files by `SifliWireCodec` itself.

## Work completed in the code

- `SifliWireCodec`: stand-alone JVM-testable packer for START, FILE_START, DATA, END_FILE, END; serial fragmentation/reassembly; opcode/status extraction; CRC/MPEG-2 checker.
- `SifliFilePush` integrates this codec for the **still-disabled** uploader, reducing duplicate wire formatting logic. It now logs malformed incoming frames instead of accepting truncated reassemblies.
- Tests include concrete captured START/ACK bytes, 4096-byte block and 244-byte fragmentation round-trip, short frames, CRC reference and corruption, invalid lengths, incomplete and orphan fragments.
- **Not resolved by this change:** exact Android `WRITE_NO_RESPONSE` flow-control, two-GATT-client contention, multi-packet retry/backpressure semantics or type-3 customization. These must remain explicit release blockers. `isUploadProtocolVerified()` must continue returning false.

## Offline recovered .so ABI (original manufacturer face)

Verified via `readelf -h -s -d` after excluding the four-byte CRC trailers:

| Item | Stock `wf_clock23.so` | Stock `wf_clock23_res.so` |
|---|---|---|
| ELF class | 32-bit | 32-bit |
| Encoding | little-endian | little-endian |
| ELF type | ET_DYN / shared | ET_DYN / shared |
| Architecture | ARM, EM_ARM=40 | ARM, EM_ARM=40 |
| Flags | `0x5000400` | `0x5000400` |
| ABI | ARM EABI v5, hard-float | ARM EABI v5, hard-float |
| Symbol `module_init` | Defined | Not assumed |
| Symbol `module_cleanup` | Defined | Not assumed |
| Symbol `wf_clock23_register` | Defined | Not assumed |
| Symbol `lv_i18n_lang_pack` | Imported | Defined |

Other imported symbol names of the OEM executable include `app_tileview_register_watchface`, `wf_view_init`, `rt_malloc`, `rt_free`, `lv_cont_create`, `lv_obj_align`, and `dlmodule_set_res_mng`. Their presence proves an ABI dependency, **not** function signatures or an independent extension API.

### Descriptor `wf_clock23.dsc` (observed, no invented field semantics)

Total transferred size: 88 bytes (84 bytes descriptor body followed by 4-byte CRC). The 84-byte body has these tagged length-prefixed zero-terminated ASCII values:

| Tag | Length | Value |
|---|---:|---|
| 0x06 | 11 | `wf_clock23\0` |
| 0x0a | 11 | `wf_clock23\0` |
| 0x08 | 14 | `wf_clock23.so\0` |
| 0x09 | 18 | `wf_clock23_tn.bin\0` |
| 0x07 | 18 | `wf_clock23_res.so\0` |
| 0x00 | 0 | Terminator |

CRC bytes in the captured descriptor: `ab 41 1b 46` (little-endian).
The tag meanings, acceptance checks and constraints on third-party descriptor names remain unknown.

## Experimental Monolith ABI probe

`watchface/prototypes/monolith_abi_probe.c` is an intentionally minimal, **non-installable** ARM module with `module_init`, `module_cleanup`, and `monolith_watchface_probe_version` exports. It calls neither LVGL nor RT-Thread, because required signatures and registration conventions are not yet verified. It is **not** a real watchface and must **never** be sent to the watch.

To compile on a Linux host with Clang/LLD and binutils:

```sh
bash watchface/prototypes/build_abi_probe.sh /tmp/monolith_abi_probe.so
```

A local offline compile of the same C content produced ELF32 ARM ET_DYN with EABI5 hard-float flags `0x5000400`. **ABI-header compatibility is not runtime compatibility.** CI separately performs the same static build and header check.

## Next acceptance gates

1. **CI:** record green Python/JVM, Android APK build, and local ARM ABI prototype compilation against the exact PR head. No pass claims based only on earlier commits.
2. **Wire-level:** introduce controlled, non-writing replay/simulator to test ACK/error/retry and finite reassembly end-to-end.
3. **Native ABI:** obtain matching SiFli SDK headers or verified export signatures; build module with actual watchface registration. Never guess function prototypes.
4. **Safety:** determine safe rollback/recovery and verify Android GATT flow-control before any user-content upload.
5. **Hardware:** after above checks, carry out first authorized, reversible user-content test under a written procedure.

The HCI transcript and OEM binaries stay outside the public repository.
