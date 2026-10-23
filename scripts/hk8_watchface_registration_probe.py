#!/usr/bin/env python3
"""Offline audit of the exact OEM HK8 wf_clock23.so stripped of its CRC footer.

The machine-instruction addresses below come from ARM Thumb disassembly of the
authorized OEM sample. Exact SHA-256 is enforced so the address map can never
be silently applied to a different binary. Nothing runs on the watch.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

OEM_SHA256 = "a037add2606e114e27705e2bdb0dd17e220fa32d2f1b712dfffd83697037bfec"


def thumb_pc_relative_add(addr, word):
    signed = word if word < 0x80000000 else word - 0x100000000
    return (addr + 4 + signed) & 0xFFFFFFFF


def tbb_destinations(table_address, values):
    return [table_address + 2 * v for v in values]


def elf_sections(blob):
    if blob[:6] != b"\x7fELF\x01\x01":
        raise ValueError("ELF32 little-endian expected")
    offset = struct.unpack_from("<I", blob, 32)[0]
    stride, count, string_idx = struct.unpack_from("<HHH", blob, 46)
    hs = [struct.unpack_from("<10I", blob, offset+i*stride) for i in range(count)]
    raw = blob[hs[string_idx][4]:hs[string_idx][4]+hs[string_idx][5]]
    def name(i):
        start = hs[i][0]
        return raw[start:raw.find(b"\0", start)].decode("ascii")
    return {name(i): {"address": h[3], "file_offset":h[4], "size":h[5]}
            for i,h in enumerate(hs)}


def inspect(path):
    blob=Path(path).read_bytes()
    digest=hashlib.sha256(blob).hexdigest()
    if digest != OEM_SHA256:
        raise ValueError("not the verified HK8 OEM sample; refusing address-based inference")
    sections=elf_sections(blob)
    def mem(addr,n):
        for s in sections.values():
            if s["address"] <= addr and addr+n <= s["address"]+s["size"]:
                off=s["file_offset"]+addr-s["address"]
                return blob[off:off+n]
        raise ValueError("unmapped virtual address: "+hex(addr))
    def u32(addr):
        return struct.unpack("<I",mem(addr,4))[0]
    def ascii_at(addr):
        ro=sections[".rodata"]
        if not ro["address"]<=addr<ro["address"]+ro["size"]:
            raise ValueError("not rodata: "+hex(addr))
        return mem(addr,ro["address"]+ro["size"]-addr).split(b"\0",1)[0].decode("ascii")
    dy=sections[".dynsym"]
    st=sections[".dynstr"]
    pool=blob[st["file_offset"]:st["file_offset"]+st["size"]]
    symbols=[]
    for i in range(dy["size"]//16):
        off=dy["file_offset"]+16*i
        name,value,size,info,other,shndx=struct.unpack_from("<IIIBBH",blob,off)
        symbols.append({"name":pool[name:pool.find(b"\0",name)].decode("ascii"),
                        "value":value,"size":size,"shndx":shndx})
    rel=sections[".rel.dyn"]
    relocations={}
    for off in range(rel["file_offset"],rel["file_offset"]+rel["size"],8):
        where,info=struct.unpack_from("<II",blob,off)
        if (info&255)==21:
            relocations[where]=symbols[info>>8]["name"]
    got=thumb_pc_relative_add(0x7da,u32(0x7f8))
    if got!=sections[".got"]["address"]:
        raise ValueError("GOT calculation disagrees with ELF")
    def function_at_literal(literal):
        return relocations.get(got+u32(literal),"<unresolved>")
    target=function_at_literal(0x7fc)
    if target!="app_tileview_register_watchface":
        raise ValueError("registration GOT slot differs from original")
    name=ascii_at(thumb_pc_relative_add(0x7ec,u32(0x808)))
    thumb_cb=thumb_pc_relative_add(0x7e8,u32(0x800))
    thumb_path=ascii_at(thumb_pc_relative_add(0x7ea,u32(0x804)))
    targets=tbb_destinations(0x67c,mem(0x67c,4))
    if thumb_cb!=0x669 or targets!=[0x680,0x720,0x742,0x760]:
        raise ValueError("event dispatcher pattern does not match original")
    return {
       "offline_only":True,
       "sha256":digest,
       "registration":{"callee":target,"r0_name":name,"r1_integer":0,
                       "r2_thumbnail":thumb_path,"r3_thumb_handler":hex(thumb_cb),
                       "event_switch_targets":[hex(x) for x in targets],
                       "C_types_verified":False},
       "init_calls":[function_at_literal(x) for x in
                     [0x85c,0x860,0x868,0x86c,0x870,0x874]],
       "init_rt_malloc_bytes":44,
       "cleanup_calls":[function_at_literal(x) for x in [0x8a8,0x8ac,0x8b0]],
       "ui_event0":{"context_parent_offset":0x24,"width":410,"height":494,
                    "called_imports":[function_at_literal(x) for x in
                       [0x78c,0x790,0x794,0x798,0x79c,0x7a0,0x7a4,0x7ac,0x7b0]]},
       "events_inferred_from_control_flow":{"0":"create","1":"resume",
                                           "2":"pause","3":"destroy"},
       "not_proven":["external C prototypes","third-party loader acceptance",
                     "module execution","safe rollback and recovery"]
    }


if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("oem_crc_stripped_so")
    args=ap.parse_args()
    print(json.dumps(inspect(args.oem_crc_stripped_so),indent=2,ensure_ascii=False))
