#!/usr/bin/env python3
"""Strict forensic SiFli legacy eZIP block-table parser (does not decode pixels).

Dissect the real Monolith 485x520 file without guessing the shared Huffman
variant. Never mistake a valid block table for decoded image data.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path

MONOLITH_SHA="e4d2412b8c2736fffe9b68f729d6cbbd1b1baefe4f68f2010c16bf60bfc03d9b"
MAX_COMPRESSED_BYTES=32*1024*1024
MAX_BLOCKS=8192

class EzipFormatError(ValueError):
    pass

def parse(blob:bytes, exact_monolith:bool=False)->dict:
    if len(blob)<28 or len(blob)>MAX_COMPRESSED_BYTES:
        raise EzipFormatError("truncated/oversized eZIP")
    sha=hashlib.sha256(blob).hexdigest()
    if exact_monolith and sha!=MONOLITH_SHA:
        raise EzipFormatError("original Monolith file hash mismatch")
    header=struct.unpack_from("<I",blob,0)[0]
    fmt=header&31
    w=(header>>10)&2047;h=(header>>21)&2047
    if not w or not h or fmt not in (1,2):
        raise EzipFormatError("unsupported image header")
    # Original 4-byte LVGL header + inner big-endian 16-byte eZIP header.
    z=memoryview(blob)[4:]
    declared=struct.unpack_from(">I",z,0)[0]
    control=z[4];depth=z[5];block_rows=z[6]
    inner_w,inner_h=struct.unpack_from(">HH",z,8)
    if declared!=len(z) or (w,h)!=(inner_w,inner_h):
        raise EzipFormatError("inner stream size/dimensions mismatch")
    if not block_rows or block_rows>h or depth not in (8,16,24,32):
        raise EzipFormatError("unsupported block rows / pixel depth")
    rows,n=struct.unpack_from(">HH",z,16)
    if rows!=block_rows or n<1 or n>MAX_BLOCKS or n!=(h+block_rows-1)//block_rows:
        raise EzipFormatError("incorrect block count")
    table_end=20+4*(n-1) # n start offsets, starting at z[20]
    if table_end>len(z):
        raise EzipFormatError("truncated offset table")
    offsets=[struct.unpack_from(">I",z,20+4*i)[0] for i in range(n)]
    offsets[0]&=0x00ffffff
    if sorted(offsets)!=offsets or any(v<=table_end or v>=len(z) for v in offsets):
        raise EzipFormatError("overlapping/invalid compressed block offsets")
    starts=offsets
    boundaries=offsets[1:]+[len(z)]
    blocks=[{"index":i,"rows":min(block_rows,h-i*block_rows),
             "start":start,"end":end,"bytes":end-start,
             "first_8_hex":bytes(z[start:start+8]).hex()}
           for i,(start,end) in enumerate(zip(starts,boundaries))]
    if any(b["bytes"]<=0 for b in blocks):
        raise EzipFormatError("empty compressed block")
    return {
      "status":"BLOCK_TABLE_VERIFIED__PIXELS_NOT_DECODED",
      "source_sha256":sha,"size":len(blob),
      "width":w,"height":h,"lvgl_format":fmt,
      "inner_declared_size":declared,"control_byte":control,"bit_depth":depth,
      "rows_per_block":block_rows,"block_count":n,
      "offset_table_end":table_end,"shared_header_bytes":offsets[0]-table_end,
      "blocks":blocks,
      "decoded_pixel_bytes":None,
      "decoded_png_path":None,
      "firmware_compatibility":"UNVERIFIED",
      "safe_to_upload":False,
    }

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bin")
    ap.add_argument("--require-original-monolith",action="store_true")
    ap.add_argument("--json",help="write JSON report")
    a=ap.parse_args()
    report=parse(Path(a.bin).read_bytes(),a.require_original_monolith)
    body=json.dumps(report,ensure_ascii=False,indent=2)
    if a.json:Path(a.json).write_text(body+"\n",encoding="utf8")
    print(body)
if __name__=="__main__":main()
