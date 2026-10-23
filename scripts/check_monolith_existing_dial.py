#!/usr/bin/env python3
"""Verify the EXISTING Monolith design before any watchface resource integration.

No scaling, re-encoding, packaging, BLE or on-watch writes.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXPECTED_LEN=158937
EXPECTED_HEADER=bytes.fromhex("01940741")
EXPECTED_WIDTH=485
EXPECTED_HEIGHT=520
WATCHFACE_ROOT=(410,494)
HYPOTHETICAL_REMOTE="/ex/resource/monolith/clock_bg.bin"

def inspect(path):
    b=Path(path).read_bytes()
    if len(b)!=EXPECTED_LEN or b[:4]!=EXPECTED_HEADER:
        raise ValueError("The file is not the preserved Monolith source eZIP resource")
    h=struct.unpack("<I",b[:4])[0]
    w=(h>>10)&0x7ff
    height=(h>>21)&0x7ff
    fmt=h&31
    if (w,height,fmt)!=(EXPECTED_WIDTH,EXPECTED_HEIGHT,1):
        raise ValueError("Invalid Monolith eZIP LVGL header")
    return {"width":w,"height":height,"format_id":fmt,"size_bytes":len(b),
      "sha256":hashlib.sha256(b).hexdigest(),
      "device_root":{"width":410,"height":494},
      "overflow_px":{"right":w-410,"bottom":height-494},
      "candidate_path_not_verified":HYPOTHETICAL_REMOTE,
      "render_status":"NOT VERIFIED; no scaling or cropping policy adopted",
      "safe_to_flash":False}

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("existing_monolith_ezip")
    args=p.parse_args()
    print(json.dumps(inspect(args.existing_monolith_ezip),indent=2))
