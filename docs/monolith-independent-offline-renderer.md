# MONOLITH independent offline watchface renderer — PHASE 05

**Outcome:** a functioning *offline browser renderer* is implemented for the
**original** Monolith/DKRNN design, without re-encoding or repainting it.
This is **not** executable firmware or an LVGL hardware emulator.

## Exact source and test data

Original PNG: \`dial/monolith/clock_bg.png\` from branch
\`claude/project-thread-3o43p6\`:
- width=485, height=520; 8-bit RGB
- source SHA-256 \`53a65f602d7266e1e6b54279a40439a828d7bb51d05ecf0a33b97b37c01186bf\`

Identical original eZIP:
\`overlay/app/src/main/assets/monolith/clock_bg.bin\`:
- 158937 bytes; LVGL header type=1 width=485 height=520
- SHA-256 \`e4d2412b8c2736fffe9b68f729d6cbbd1b1baefe4f68f2010c16bf60bfc03d9b\`

\`build_offline_renderer.py\` validates both formats, includes the PNG in
a **completely self-contained, network-free HTML**, and optionally validates
the exact original eZIP file. It does not claim to decode the latter.

## Usage (no additional packages, off-watch)

From repo checkout:

\`\`\`bash
python3 watchface/renderer/build_offline_renderer.py \
  --original-png dial/monolith/clock_bg.png \
  --original-ezip overlay/app/src/main/assets/monolith/clock_bg.bin \
  --output monolith-offline-renderer.html
\`\`\`

The original PNG lives in \`claude/project-thread-3o43p6\` and is not present
in every working branch. **If the file is absent, pass the actual path
to the original file from that branch**; do not substitute an unrelated PNG.
The eZIP is present in the Android assets on this working branch.

Open the generated HTML locally in a browser. It provides:
- live system time/date overlay on top of the *unaltered original artwork*;
- the complete original canvas 485×520;
- display geometry variants: no-scale center crop, contain, cover at 410×494;
- an export button saving the current rendered frame to PNG;
- provenance/format SHA-256 metadata.

These 410×494 display modes are **experimental visual options**, **not
manufacturer-verified rendering behavior or approval to crop the brand art**.

## Tests actually executed

- \`python3 -m unittest discover -s tests -p test_monolith_offline_renderer.py -v\`: 3 PASS;
- actual original 485×520 PNG and original 158937-B BIN were used to build
  a 225371-byte independent HTML file;
- CI runs the same three synthetic tests.

## What this does NOT implement

- \`clock_bg.bin\` **has not been decoded** into pixels. OpenEZIP's simplistic
  raw-DEFLATE function rejected this sample. The verified source PNG is used
  directly. Saying otherwise would misrepresent the work.
- The HTML/Canvas renderer is **not LVGL itself**, does not execute a SiFli
  EPIC/EZIP hardware decoder, and does not support deployment on HK8 3407.
- Firmware's \`wf_obj_img_create\`, \`app_tileview_register_watchface\` and
  custom-module acceptance remain unresolved.
- No proprietary image decoder or vendor resource manager has been replaced
  in the physical watch; no BLE writes, firmware changes or upload unlocking.

**Next engineering test:** decode the exact Monolith eZIP into pixels with
validated SiFli hardware/legacy stream support and compare decoded RGB against
the original PNG at pixel level, *before* claiming a compatible eZIP renderer.
