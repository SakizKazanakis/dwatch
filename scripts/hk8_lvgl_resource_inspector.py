#!/usr/bin/env python3
"""Read-only SHA-pinned OEM HK8 3407 resource descriptor inspector.

Extracts only known raw words, ARM relocation classes and referenced filenames.
Never assumes C layout, packed field semantics, rendering or loader support.
No private binaries are committed to this repository.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

OEM_SHA256 = "a037add2606e114e27705e2bdb0dd17e220fa32d2f1b712dfffd83697037bfec"

def sections(blob):
    if blob[:6] != b"\x7fELF\x01\x01":
        raise ValueError("ELF32 little-endian required")
    shoff=struct.unpack_from("<I",blob,32)[0]
    step,count,table_idx=struct.unpack_from("<HHH",blob,46)
    headers=[struct.unpack_from("<10I",blob,shoff+i*step) for i in range(count)]
    names=blob[headers[table_idx][4]:headers[table_idx][4]+headers[table_idx][5]]
    out={}
    for h in headers:
        n=h[0]
        name=names[n:names.find(b"\0",n)].decode("ascii") if n<len(names) else ""
        out[name]=(h[3],h[4],h[5])
    return out

def words_at(blob,secmap,address,count):
    for addr,off,size in secmap.values():
        if size and addr<=address and address+4*count<=addr+size:
            return list(struct.unpack_from("<"+"I"*count,blob,off+address-addr))
    raise ValueError("address outside mapped ELF section: "+hex(address))

def inspect(path):
    b=Path(path).read_bytes()
    digest=hashlib.sha256(b).hexdigest()
    if digest != OEM_SHA256:
        raise ValueError("different OEM ELF; fixed-address evidence not transferable")
    sec=sections(b)
    def words(addr,n):return words_at(b,sec,addr,n)
    def string(addr):
        base,off,size=sec[".rodata"]
        if not base<=addr<base+size:raise ValueError("not a rodata string")
        pos=off+addr-base
        return b[pos:pos+(base+size-addr)].split(b"\0",1)[0].decode("ascii")
    image=words(0xaf0,3)
    analog=words(0xafc,3)
    linked=words(0x9ec,9)
    if image != [6,0,0x8bf] or analog != [0xcd0005,0xf7,0x9ec]:
        raise ValueError("descriptor changed")
    if linked != [0x8e9,0x912,0x7e000f,0x941,0x912,0xca000f,0x96a,0x993,0xca0006]:
        raise ValueError("linked resource list changed")
    rel=sec[".rel.dyn"]
    rels=b[rel[1]:rel[1]+rel[2]]
    relocation={addr:kind&255 for addr,kind in struct.iter_unpack("<II",rels)}
    ptrs=(0xaf8,0xb04,0x9ec,0x9f0,0x9f8,0x9fc,0xa04,0xa08)
    if any(relocation.get(addr)!=23 for addr in ptrs):
        raise ValueError("missing R_ARM_RELATIVE pointer relocation")
    return {
       "sha256":digest,"private_fixture_only":True,
       "image":{"virtual_address":"0xaf0","raw_words":[hex(w) for w in image],
                "path":string(image[2]),"C_struct":"UNKNOWN"},
       "analog":{"virtual_address":"0xafc","raw_words":[hex(w) for w in analog],
                 "linked_table":"0x9ec","C_struct":"UNKNOWN"},
       "linked_records":[
          {"resource":string(linked[i]),"aux_resource":string(linked[i+1]),
           "packed_u32":hex(linked[i+2]),"semantics":"UNKNOWN"} for i in (0,3,6)],
       "verified_relocations":{hex(x):"R_ARM_RELATIVE" for x in ptrs},
       "not_verified":["C layout","encoded dimensions/colours","runtime rendering",
                       "custom-resource compatibility","safe flash/recovery"]
    }

if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("oem_crc_stripped_so")
    print(json.dumps(inspect(ap.parse_args().oem_crc_stripped_so),indent=2))
