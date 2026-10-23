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
- No `/ex/` literal was found in the inspected `classes6.dex`. This **does
  not prove** the `/ex/` path is unsupported: it may be constructed elsewhere
  or belong to a different device/SDK.
- The string `3407` was **not found** in the inspected `classes6.dex`;
  code 3407 is known from the physical Wearfit Pro device-information screen,
  not this APK's literals.

This is a concrete static cross-reference, **not** reconstructed method behavior
or captured upload packets. The uploader therefore stays disabled.

## Safe acceptance sequence

1. Build and run static tests plus the platform-independent Java profile tests on CI.
2. Install the CI-built APK on a phone, select this watch in Monolith and press **BLE-диагностика (только чтение)**.
3. Export the resulting text with **Экспорт BLE-отчёта**; check expected UUIDs, client statuses, and disconnect handling.
4. Only after evidence of the actual resource framing and error paths is available, rework the transmitter; preserve the fail-closed upload gate.
5. Never infer a safe system firmware flashing method from GATT capability discovery.

**Current release policy:** do not merge or publish the diagnostic as hardware-verified before an on-watch read-only probe. Do not enable file uploads until protocol compatibility and recovery are demonstrated.
