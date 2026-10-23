# HK8 PRO MAX 3407 — BLE discovery and protocol evidence

## Reference watch (observed by user, not a runtime GATT read)

| Parameter | Observed |
|---|---|
| Device / product | HK8 PRO MAX / HK ONE |
| Wearfit Pro Code | 3407 |
| System / firmware | 2.09 / FW_V2.0.3 |
| Other version strings | TG0733-2 / WF_V6 / BT2.24.1 |
| BLE device address | C8:00:00:AB:D5:CC (one test unit; not a hardware-wide identifier) |
| SiFli GATT service | 00000000-0000-0000-6473-5f696c666973 |
| SiFli data characteristic | 00000000-0000-0200-6473-5f696c666973 |
| Wearfit UART-like service | 6e400001-b5a3-f393-e0a9-e50e24dcca9e |
| Wearfit RX / writable | 6e400002-b5a3-f393-e0a9-e50e24dcca9e |
| Alternate service | 6e400011-b5a3-f393-e0a9-e50e24dcca9e |
| Alternate writable characteristic | 6e400012-b5a3-f393-e0a9-e50e24dcca9e |
| Alternate notify characteristic | 6e400013-b5a3-f393-e0a9-e50e24dcca9e |

## Implemented read-only diagnostic

The **BLE-диагностика (только чтение)** button creates one transient Android BLE GATT client, discovers the services/characteristics/descriptors, records callback status codes, labels known UUIDs and exports the report via explicit Android Share action. It never writes characteristic or descriptor values, reads characteristic values, subscribes to notifications, or requests a new MTU. The separate GATT client may still conflict with a concurrently connected app.

Its profile status **AVAILABLE (UUID only)** proves the UUID exists in the GATT table. It does **not** prove that any command is supported, that resources can be installed, or that executable modules are loadable.

No notifications / data payloads are captured. **The diagnostic does not intercept Wearfit Pro traffic.** To establish exact command framing and responses, capture authorized Bluetooth HCI logs of a *real* Wearfit Pro stock-watchface transaction, or obtain a matched reference recording; no guessed packets must be transmitted.

## Protocol comparison and evidence gaps

| Component | Monolith code / available source | Verified on HK8 3407? |
|---|---|---|
| BLE serial characteristic | SiFli 0x0200 GATT UUID | **YES: GATT metadata only** |
| Wearfit command channel | Nordic UART-like 0x0002 characteristic | **YES: GATT metadata only** |
| Notifications | SiFli 0x0200 has NOTIFY and CCCD | **YES: characteristic property/descriptor only** |
| Outer packet category=4, flags 0/1/2/3 | `SifliFilePush.send` | **NO:** no packet recording |
| Upload commands 0/2/4/6/8/13 | `SifliFilePush.run` | **NO** |
| CRC32/MPEG2 plus 4-byte alignment | local implementation + standalone reference check | **Algorithm only; not actual watch payload** |
| Resource directory `/ex/resource` vs `/dyn/resource` | current UI upload (disabled) | **NO** |
| Custom dial handshake 0xAD and clock style 0xAB / 0xEA | code from Wearfit reconstruction | **NO** |
| Sifli OTA V3 resource/app types | [SiFli OTA Android documentation](https://docs.sifli.com/projects/solution/mobile-sdk/ota/ota_v3_android.html) | **Documented for SiFli SDK; watch compatibility not verified** |
| Dynamic .so module / GUIBuilder | [SiFli watchface framework](https://docs.sifli.com/projects/solution/2.application-notes/ui/wf.html) | **NO** |

The previously provided Wearfit Pro APK was statically inspected and includes classes with names such as `SifliWatchfaceService`, `SicheTransferMgr`, `SicheOtaMgr`, `SFWatchfaceFilePushManager`, and `CRC32MPEG2`. Their presence alone does not prove they execute for device code 3407, nor specify the exact on-wire protocol. The actual method bodies and a live transfer trace remain to be matched. **No device packet capture or firmware extraction has been performed.**

## Direct Wearfit Pro APK check (local static inspection)

Inspected the user-supplied `wearfit.zip` containing Wearfit Pro `com.wakeup.howear`
`hw_5.6.17` / `2026101619`. From `classes6.dex` string IDs:

- `/dyn/resource/` and `/dyn/resource/ai_clock_crop_bg.bin` are literal strings.
- `isNewPath`, `getBoardType`, `startSendPic`, `startSendPicAi`, and
  `pushWatchfaceFile` appear as method names.
- `ctrl_packet.bin` appears in the APK, but is not evidence of a locally
  obtained HK8 3407 firmware image.
- **Correction (verified by method-level DEX inspection):** `classes6.dex`
  includes the concrete paths `ex/resource/clock_bg.bin`,
  `dyn/resource/clock_bg.bin`, `ex/clock_bg.zip`, and
  `/dyn/resource/ai_clock_crop_bg.bin`. A previous string-only check that
  reported no `/ex/` path was insufficient and is superseded.
- The string `3407` was **not found** in the inspected `classes6.dex`;
  code 3407 is known from the physical Wearfit Pro device-information screen,
  not this APK's literals.

This is a concrete static cross-reference, **not** reconstructed method behavior
or captured upload packets. The uploader therefore stays disabled.

## PHASE 03 — direct DEX method inspection (23 October 2026)

The attached Wearfit Pro base APK SHA-256 is
`ec085779aac38d130171cf536c76eaa409729b9a073719a5c80ee9c9c4f3d588`.
The following findings come from reading Dalvik method definitions/instructions,
**not just strings or matching method names**.

1. `classes5.dex`:
   - `com.wakeup.common.utils.SicheUtils.isNewPath(int):boolean` returns
     **true** if and only if the input is **5, 8, or 13**.
   - `SicheUtils.getBoardType(int):int` maps **4→1**, **8→1**,
     **5→2**, **13→3**, and all other inputs to **0**.
   - These mappings were reconstructed from branch-targeted DEX instructions
     in the methods (16 and 22 code units respectively).
2. `classes6.dex`:
   - `SicheTransferMgr2.siCheCustomDial` and
     `SicheTransferMgr.siCheCustomDial` call
     `DeviceLocalSupports.getType()`, then pass that integer to
     `SicheUtils.isNewPath(int)` and `SicheUtils.getBoardType(int)`.
   - In `SicheTransferMgr2.siCheCustomDial`, the `isNewPath` result
     selects `dyn/resource/clock_bg.bin` when true, or
     `ex/resource/clock_bg.bin` when false (DEX branch at code unit 61,
     strings at code units 63 and 66). The method builds
     `ex/clock_bg.zip` as a local output and invokes
     `sifliEzipUtil.pngToEzip` with pixel format `rgb888`.
   - `SicheTransferMgr.siCheCustomDial` also references
     `rgb888`, `ex/resource/`, `dyn/resource/` and the same
     `isNewPath/getBoardType` logic; it also handles thumbnail files.
   - `SicheTransferMgr.startPushDiyDial` obtains
     `DeviceLocalSupports.getType()` and uses `getBoardType`;
     it references `rgb888a` for a different resource path.
3. **Critical disambiguation:** The numeric argument used by these methods
   is a locally stored **DeviceLocalSupports type**, *not* the displayed
   Wearfit **Code 3407**. No evidence currently maps code `3407` to
   type `4`, `5`, `8`, or `13`. Therefore the *actual* upload
   directory, boardType and zip layout for this watch remain **UNKNOWN**.
4. In `classes2.dex`, the SiFli SDK method
   `SFWatchfaceFilePushManager.pushWatchfaceFile(String,int,String,boolean,int)`
   validates the input ZIP path, checks `isBusy`, and invokes
   `UnzipHelper.unzipAsync`. Its embedded validation error is
   `maxFileSliceLength is not valid.(1-10)*1024 required`.
   Another overload calls `SFPushInfoMaker.makePushInfos` for a
   file list. This is direct evidence of ZIP-based file push and a
   1–10 KiB slice-size constraint **in the SDK**, not evidence that the
   watch accepted any particular command or slice size.
5. `SicheTransferMgr2.siCheCustomDial` prepares `ex/clock_bg.zip`
   and references both paths as alternative destination names.
   **Sending duplicate files to both destinations** (as the earlier
   Monolith prototype attempted) does not reproduce this conditional
   decision.

6. The existence of path/codec logic **does not** demonstrate that the
   installed HK8 firmware permits external file writes or .so execution.
   The upload guard stays disabled.

**Evidence limits:** This method-level decoding proves the control-flow
of the named functions. No Wearfit Pro transfer was sniffed from the
physical watch and no live selection of `DeviceLocalSupports.getType()`
was observed.

## Safe acceptance sequence

1. Build and run static tests plus the platform-independent Java profile tests on CI.
2. Install the CI-built APK on a phone, select this watch in Monolith and press **BLE-диагностика (только чтение)**.
3. Export the resulting text with **Экспорт BLE-отчёта**; check expected UUIDs, client statuses, and disconnect handling.
4. Only after evidence of the actual resource framing and error paths is available, rework the transmitter; preserve the fail-closed upload gate.
5. Never infer a safe system firmware flashing method from GATT capability discovery.

**Current release policy:** do not merge or publish the diagnostic as hardware-verified before an on-watch read-only probe. Do not enable file uploads until protocol compatibility and recovery are demonstrated.
