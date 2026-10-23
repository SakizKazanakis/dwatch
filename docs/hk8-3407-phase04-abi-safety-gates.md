# PHASE 04.2 — verified OEM watchface ABI and blocked interfaces

This document is grounded in the recovered stock `wf_clock23.so` from the owner's completed Wearfit Pro HCI transaction, inspected with `readelf -h -Ws -d` on 2026-10-23. OEM binaries and raw HCI logs **are not** committed.

## Verified binary properties

```text
Class: ELF32
Data: little-endian
Type: DYN (Shared object file)
Machine: ARM
Flags: 0x5000400, Version5 EABI, hard-float ABI
```

From the actual stock dynamic symbol table:

| Symbol | Binding | Type | Value | Size | Meaning that can be asserted |
|---|---|---|---|---:|---|
| `wf_clock23_register` | GLOBAL | FUNC | `0x000007d5` | 56 | OEM module exports a registration-related function |
| `module_init` | GLOBAL | FUNC | `0x0000080d` | 108 | OEM module exports a loader entry |
| `module_cleanup` | GLOBAL | FUNC | `0x00000879` | 60 | OEM module exports a cleanup entry |
| `wf_view_init` | GLOBAL | NOTYPE | UND | — | Unresolved external called by OEM module |
| `lv_cont_create` | GLOBAL | NOTYPE | UND | — | LVGL dependency |
| `app_tileview_register_watchface` | GLOBAL | NOTYPE | UND | — | OEM registration dependency |
| `dlmodule_set_res_mng` | GLOBAL | NOTYPE | UND | — | OEM resource binding dependency |

Dynamic section of the recovered `wf_clock23.so` contains `DT_HASH`, `DT_STRTAB`, `DT_SYMTAB`, `DT_STRSZ=531`, `DT_SYMENT=16`, `DT_REL`, `DT_RELSZ=264`, `DT_RELENT=8`, `DT_RELCOUNT=8`, `DT_NULL`. No `DT_NEEDED` was present in the inspected section.

**Proof boundary:** `readelf` establishes exported and imported *names*, relocations and object ABI. It does **not** reveal C prototypes, calling conventions for each external (beyond the module-wide ABI), opaque structures, register call ordering, or compatibility with the target loader. A test ELF whose header and `module_init` match is NOT a verified runnable watchface.

## Why a full native module is still gated

- Exact signature and required invocation order of `app_tileview_register_watchface` are unknown. Binding the wrong function prototype can corrupt runtime state.
- `wf_view_init` and `dlmodule_set_res_mng` may require OEM-specific structures or resource ownership conventions not represented in the public generic SiFli SDK.
- No verified third-party module installation, rollback, or emergency recovery procedure for HK ONE code 3407 / firmware 2.09.
- The successful OEM watchface transfer is only evidence that **official packaged files** were accepted. It is not permission/proof for arbitrary `.so` execution.

## Implemented safe transport correction in this branch

- Unexpected or stale response opcode now fails the pending `SifliFilePush.await()` rather than silently losing it and timing out.
- Nonzero GATT connection statuses immediately wake the waiting writer with a failure status.
- Pure Java `SifliWireCodecJvmTest` now explicitly checks stale opcode, rejection status, truncated response and accepted ACK.
- **No write-unlocking code is present.** `isUploadProtocolVerified()` remains `false`; UI upload button remains disabled.
- Android `WRITE_TYPE_NO_RESPONSE` flow-control assumptions and retry handling remain unverified and should not be modified by guessing.

## Required evidence for next functional module

Obtain firmware-matched symbols *with prototypes* or safely reconstruct the registration ABI from isolated static disassembly; verify the `.dsc` and resource link constraints; build a host simulator of module registration; only then consider a signed-off reversible physical test. No unknown code or OTA firmware writing as part of this stage.
