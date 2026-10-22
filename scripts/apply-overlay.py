#!/usr/bin/env python3
"""Copies overlay/ into a Gadgetbridge checkout and makes Monolith the launcher.

Usage: scripts/apply-overlay.py <path-to-gadgetbridge-checkout>
"""
import re
import shutil
import sys
from pathlib import Path

ACTIVITY = """        <activity
            android:name="nodomain.freeyourgadget.gadgetbridge.monolith.MonolithActivity"
            android:label="@string/app_name"
            android:theme="@style/MonolithTheme"
            android:launchMode="singleTop"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
"""


def main() -> None:
    gb = Path(sys.argv[1]).resolve()
    overlay = Path(__file__).resolve().parent.parent / "overlay"

    for src in overlay.rglob("*"):
        if src.is_file():
            dst = gb / src.relative_to(overlay)
            if dst.exists():
                sys.exit(f"overlay would overwrite upstream file: {dst}")
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

    manifest = gb / "app/src/main/AndroidManifest.xml"
    text = manifest.read_text(encoding="utf-8")

    # Monolith opens from the launcher; upstream's main screen stays reachable from it.
    match = re.search(r'<activity\s[^>]*android:name="\.activities\.ControlCenterv2".*?</activity>', text, re.S)
    if match:
        block = match.group(0)
        stripped = re.sub(r'\s*<category android:name="android\.intent\.category\.LAUNCHER"\s*/>', "", block)
        text = text.replace(block, stripped)
    else:
        print("warning: ControlCenterv2 not found, upstream launcher kept", file=sys.stderr)

    if "</application>" not in text:
        sys.exit("</application> not found in manifest")
    text = text.replace("</application>", ACTIVITY.lstrip() + "    </application>", 1)
    manifest.write_text(text, encoding="utf-8")
    print("Overlay applied, MonolithActivity is the launcher")


if __name__ == "__main__":
    main()
