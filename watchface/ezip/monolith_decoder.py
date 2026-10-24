#!/usr/bin/env python3
"""Strict, dependency-free decoder for the verified Monolith legacy shared-Huffman eZIP.

Header, stream, PNG filters and BGR888 are checked. This targets the exact
original 485x520/8-bit specimen, NOT arbitrary SiFli firmware formats.
Algorithm independently implemented from observed layout, DEFLATE canonical
coding and the OpenEZIP proof of concept (github.com/xeossys/OpenEZIP).
"""
import argparse
import hashlib
import json
import math
import struct
import zlib
from pathlib import Path

ORIGINAL_SHA = "e4d2412b8c2736fffe9b68f729d6cbbd1b1baefe4f68f2010c16bf60bfc03d9b"
LEN_BASE=(3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258)
LEN_EXTRA=(0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0)
DIST_BASE=(1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577)
DIST_EXTRA=(0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13)
ORDER=(16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15)
WIDTH,HEIGHT=485,520
MAX_OUTPUT=HEIGHT*(WIDTH*3+1)

class FormatError(ValueError): pass

class Bits:
    def __init__(self, data):
        self.data=data
        self.pos=0
    def read(self, n):
        if n<0 or self.pos+n>len(self.data)*8:raise FormatError("truncated bitstream")
        out=0
        for i in range(n):
            bit=self.pos+i
            out|=((self.data[bit>>3]>>(bit&7))&1)<<i
        self.pos+=n
        return out
    def align4(self):
        self.pos=((self.pos+7)//8+3)//4*32

def tree(lengths):
    counts=[0]*16
    for n in lengths:
        if n<0 or n>15:raise FormatError("invalid Huffman code length")
        if n:counts[n]+=1
    start=[0]*16
    code=0
    for bits in range(1,16):
        code=(code+counts[bits-1])<<1
        start[bits]=code
    table={}
    for symbol,size in enumerate(lengths):
        if size:
            key=(size,start[size])
            if key in table:raise FormatError("duplicate Huffman code")
            table[key]=symbol
            start[size]+=1
    return table

def symbol(bits,table):
    val=0
    for n in range(1,16):
        val=val*2+bits.read(1)
        if (n,val) in table:return table[n,val]
    raise FormatError("unknown Huffman symbol")

def legacy_shared_huffman(data):
    bits=Bits(data)
    literal_count=bits.read(5)+257
    distance_count=bits.read(5)+1
    order_count=bits.read(4)+4
    if literal_count>286 or distance_count>32:raise FormatError("invalid Huffman table size")
    lengths=[0]*19
    for i in range(order_count):lengths[ORDER[i]]=bits.read(3)
    ct=tree(lengths)
    seq=[]
    while len(seq)<literal_count+distance_count:
        s=symbol(bits,ct)
        if s<16:seq.append(s)
        elif s==16:
            if not seq:raise FormatError("repeat without prior code")
            seq.extend([seq[-1]]*(3+bits.read(2)))
        elif s==17:seq.extend([0]*(3+bits.read(3)))
        elif s==18:seq.extend([0]*(11+bits.read(7)))
        else:raise FormatError("unknown Huffman repeat symbol")
        if len(seq)>literal_count+distance_count:raise FormatError("Huffman table overflow")
    lt=tree(seq[:literal_count]);dt=tree(seq[literal_count:])
    bits.align4()
    output=bytearray()
    while True:
        final=bits.read(1)
        blocktype=bits.read(2)
        if blocktype==0:
            bits.pos=(bits.pos+7)//8*8
            size=bits.read(16);inverse=bits.read(16)
            if size^inverse!=0xffff:raise FormatError("stored DEFLATE block integrity failure")
            for _ in range(size):
                output.append(bits.read(8))
                if len(output)>MAX_OUTPUT:raise FormatError("expanded data exceeded limit")
        elif blocktype in (1,2):
            while True:
                item=symbol(bits,lt)
                if item<256:output.append(item)
                elif item==256:break
                else:
                    index=item-257
                    if index>=len(LEN_BASE):raise FormatError("invalid length symbol")
                    count=LEN_BASE[index]+bits.read(LEN_EXTRA[index])
                    d=symbol(bits,dt)
                    if d>=len(DIST_BASE):raise FormatError("invalid distance symbol")
                    back=DIST_BASE[d]+bits.read(DIST_EXTRA[d])
                    if back<=0 or back>len(output):raise FormatError("invalid backward distance")
                    for _ in range(count):
                        output.append(output[-back])
                        if len(output)>MAX_OUTPUT:raise FormatError("expanded data exceeded limit")
                if len(output)>MAX_OUTPUT:raise FormatError("expanded data exceeded limit")
        else:raise FormatError("reserved DEFLATE block type")
        bits.align4()
        if final:break
    if len(output)!=MAX_OUTPUT:raise FormatError(f"pixel payload size {len(output)} != {MAX_OUTPUT}")
    return bytes(output)

def unfilter(packed):
    stride=WIDTH*3
    if len(packed)!=MAX_OUTPUT:raise FormatError("filtered pixel length")
    out=bytearray(HEIGHT*stride)
    filters={}
    for row in range(HEIGHT):
        pos=row*(stride+1)
        kind=packed[pos]
        filters[kind]=filters.get(kind,0)+1
        if kind>4:raise FormatError(f"invalid PNG row filter {kind} at {row}")
        for x in range(stride):
            raw=packed[pos+1+x]
            before=out[row*stride+x-3] if x>=3 else 0
            above=out[(row-1)*stride+x] if row%32!=0 else 0
            prior=out[(row-1)*stride+x-3] if row%32!=0 and x>=3 else 0
            if kind==0:add=0
            elif kind==1:add=before
            elif kind==2:add=above
            elif kind==3:add=(before+above)//2
            else:
                p=before+above-prior
                a,b,c=abs(p-before),abs(p-above),abs(p-prior)
                add=before if a<=b and a<=c else above if b<=c else prior
            out[row*stride+x]=(raw+add)&255
    # Format 1 with 3 bytes per pixel stores BGR, not RGB.
    rgb=bytearray(len(out))
    rgb[0::3]=out[2::3]
    rgb[1::3]=out[1::3]
    rgb[2::3]=out[0::3]
    return bytes(rgb),filters

def decode(data, strict_source=True):
    if len(data)<92:raise FormatError("truncated eZIP resource")
    digest=hashlib.sha256(data).hexdigest()
    if strict_source and digest!=ORIGINAL_SHA:raise FormatError("original Monolith eZIP hash mismatch")
    header,=struct.unpack_from("<I",data)
    fmt=header&31;w=(header>>10)&2047;h=(header>>21)&2047
    stream=data[4:]
    stream_len,=struct.unpack_from(">I",stream)
    sh_w,sh_h=struct.unpack_from(">HH",stream,8)
    rows,blocks=struct.unpack_from(">HH",stream,16)
    if (fmt,w,h)!=(1,WIDTH,HEIGHT) or (sh_w,sh_h)!=(WIDTH,HEIGHT):
        raise FormatError("unsupported resource pixel format or geometry")
    if (rows,blocks)!=(32,17) or stream_len!=len(stream):
        raise FormatError("unsupported shared-Huffman block layout")
    offsets=[struct.unpack_from(">I",stream,20+i*4)[0] for i in range(blocks)]
    if offsets!=sorted(offsets) or offsets[0]<88 or offsets[-1]>=len(stream):
        raise FormatError("invalid eZIP offset table")
    packed=legacy_shared_huffman(stream[88:])
    rgb,filter_counts=unfilter(packed)
    return rgb,{"source_sha256":digest,"width":w,"height":h,
      "compressed_bytes":len(data),"expanded_bytes":len(packed),
      "pixel_bytes":len(rgb),"rgb_sha256":hashlib.sha256(rgb).hexdigest(),
      "filter_counts":filter_counts,"algorithm":"legacy shared-Huffman, filter, BGR888"}

def chunk(tag,raw):
    return struct.pack(">I",len(raw))+tag+raw+struct.pack(">I",zlib.crc32(tag+raw)&0xffffffff)

def png_encode(rgb):
    if len(rgb)!=WIDTH*HEIGHT*3:raise ValueError("RGB888 length mismatch")
    stride=WIDTH*3
    rows=b"".join(b"\x00"+rgb[y*stride:(y+1)*stride] for y in range(HEIGHT))
    return b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",WIDTH,HEIGHT,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(rows,9))+chunk(b"IEND",b"")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("ezip",type=Path)
    p.add_argument("--png",type=Path,required=True)
    p.add_argument("--raw",type=Path)
    p.add_argument("--allow-other-input",action="store_true",help="development only, still requires 485x520 type 1")
    a=p.parse_args()
    rgb,info=decode(a.ezip.read_bytes(),not a.allow_other_input)
    a.png.parent.mkdir(parents=True,exist_ok=True)
    a.png.write_bytes(png_encode(rgb))
    if a.raw:a.raw.write_bytes(rgb)
    info["png_path"]=str(a.png)
    print(json.dumps(info,indent=2))

if __name__=="__main__":main()
