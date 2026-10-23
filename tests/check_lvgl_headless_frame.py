#!/usr/bin/env python3
"""Check real LVGL offscreen frame against ORIGINAL pixels decoded from eZIP."""
from __future__ import annotations
import argparse
import importlib.util
import sys
from pathlib import Path

def check(frame_path,reference_png):
    parent=Path(__file__).resolve().parents[1]/"watchface"/"ezip"
    sys.path.insert(0,str(parent))
    from decode_shared_huffman import decode_reference_png
    orig,dims=decode_reference_png(reference_png)
    if dims!=(485,520):raise AssertionError("incorrect source resolution")
    with Path(frame_path).open("rb") as f:
        magic=f.readline();geom=f.readline();mx=f.readline();rendered=f.read()
    if (magic,geom,mx)!=(b"P6\n",b"485 520\n",b"255\n"):
        raise AssertionError("headless PPM format mismatch")
    if len(rendered)!=len(orig):raise AssertionError("frame truncated")
    stride=485*3
    for ya,yb in ((0,240),(340,520)):
        a=orig[ya*stride:yb*stride];b=rendered[ya*stride:yb*stride]
        if a!=b:
            raise AssertionError(f"master DKRNN art was modified outside overlay: y={ya}:{yb}")
    a=orig[240*stride:340*stride];b=rendered[240*stride:340*stride]
    changed=sum(x!=y for x,y in zip(a,b))
    if not (1000<=changed<=10000):
        raise AssertionError(f"live time/date overlay failed: {changed} channels")
    return {"width":485,"height":520,"preserved_top_rows":240,
            "preserved_bottom_rows":180,
            "overlay_changed_channels":changed,"pixel_source":"decoded original eZIP",
            "runtime":"real public LVGL 8.3.11","HK8_hardware":"NOT VERIFIED"}

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("headless_ppm")
    ap.add_argument("reference_decoded_png")
    p=ap.parse_args()
    print("PASS:",check(p.headless_ppm,p.reference_decoded_png))
