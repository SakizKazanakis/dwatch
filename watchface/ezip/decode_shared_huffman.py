#!/usr/bin/env python3
"""Independent SiFli eZIP shared-Huffman decoder, Python standard library only.

Decodes and verifies legacy type-1 BGR888 files used by the actual MONOLITH
clock_bg.bin. Not an OEM firmware implementation and not a loader.
References: RFC 1951 canonical Huffman/LZ77, the observed device resource
header and block layout. This implementation does not import OpenEZIP code.
"""
from __future__ import annotations
import argparse,hashlib,json,math,struct,zlib
from pathlib import Path
try:
    from inspect_legacy_blocks import EzipFormatError,parse
except ImportError:
    from .inspect_legacy_blocks import EzipFormatError,parse

CANONICAL_ORDER=(16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15)
LENGTH_START=(3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258)
LENGTH_EXTRA=(0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0)
DIST_START=(1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577)
DIST_EXTRA=(0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13)
PIXEL_SHA_485X520="" # Recorded only after actual reference-pixel validation.

class DecodeError(ValueError):pass

class Bits:
    def __init__(self,data:bytes):
        self.data=data
        self.pos=0
    def read(self,n=1):
        if n<0 or n>16 or self.pos+n>len(self.data)*8:
            raise DecodeError("bitstream truncated")
        val=0
        for bit in range(n):
            val|=((self.data[(self.pos+bit)//8]>>((self.pos+bit)%8))&1)<<bit
        self.pos+=n
        return val
    def align4(self):
        self.pos=((self.pos+7)//8+3)//4*32
        if self.pos>len(self.data)*8:raise DecodeError("block alignment beyond EOF")

def huffman(lengths):
    counts=[0]*16
    for length in lengths:
        if length<0 or length>15:raise DecodeError("invalid Huffman length")
        if length:counts[length]+=1
    code=0
    first=[0]*16
    for bits in range(1,16):
        code=(code+counts[bits-1])<<1
        first[bits]=code
        if code+counts[bits]>(1<<bits):
            raise DecodeError("oversubscribed Huffman tree")
    mapping={}
    for sym,length in enumerate(lengths):
        if length:
            key=(length,first[length]);mapping[key]=sym;first[length]+=1
    if not mapping:raise DecodeError("empty Huffman tree")
    return mapping

def symbol(bits,tree):
    key=0
    for width in range(1,16):
        key=(key<<1)|bits.read()
        value=tree.get((width,key))
        if value is not None:return value
    raise DecodeError("invalid Huffman prefix")

def shared_trees(stream):
    literal_count=stream.read(5)+257
    dist_count=stream.read(5)+1
    cl_count=stream.read(4)+4
    if literal_count>286 or dist_count>32 or cl_count>19:
        raise DecodeError("bad Huffman alphabet sizes")
    cl_lengths=[0]*19
    for i in range(cl_count):
        cl_lengths[CANONICAL_ORDER[i]]=stream.read(3)
    lengths=[];codebook=huffman(cl_lengths)
    required=literal_count+dist_count
    while len(lengths)<required:
        item=symbol(stream,codebook)
        if item<=15:
            lengths.append(item)
        elif item==16:
            if not lengths:raise DecodeError("repeat with no preceding length")
            lengths.extend([lengths[-1]]*(stream.read(2)+3))
        elif item==17:
            lengths.extend([0]*(stream.read(3)+3))
        elif item==18:
            lengths.extend([0]*(stream.read(7)+11))
        else:raise DecodeError("unknown code-length token")
        if len(lengths)>required:raise DecodeError("code-length overflow")
    return huffman(lengths[:literal_count]),huffman(lengths[literal_count:])

def decode_lz77(data,max_output,shared_header_end=None):
    bits=Bits(data)
    literal_tree,dist_tree=shared_trees(bits)
    bits.align4()
    if shared_header_end is not None and bits.pos//8!=shared_header_end:
        raise DecodeError("shared table region length does not match declared block offset")
    out=bytearray()
    blocks=0
    while True:
        final=bits.read()
        typ=bits.read(2)
        blocks+=1
        if blocks>8192:raise DecodeError("too many compression blocks")
        if typ==0:
            bits.pos=((bits.pos+7)//8)*8
            length=bits.read(16);inverse=bits.read(16)
            if (length^inverse)!=65535:raise DecodeError("bad stored-block length")
            if (bits.pos//8)+length>len(bits.data):raise DecodeError("truncated stored block")
            out.extend(bits.data[bits.pos//8:bits.pos//8+length])
            bits.pos+=length*8
        elif typ in (1,2):
            while True:
                token=symbol(bits,literal_tree)
                if token<256:
                    out.append(token)
                elif token==256:
                    break
                elif 257<=token<=285:
                    i=token-257
                    run=LENGTH_START[i]+bits.read(LENGTH_EXTRA[i])
                    dist_code=symbol(bits,dist_tree)
                    if dist_code>=len(DIST_START):
                        raise DecodeError("invalid backreference distance symbol")
                    distance=DIST_START[dist_code]+bits.read(DIST_EXTRA[dist_code])
                    if distance>len(out):
                        raise DecodeError("backreference points before start")
                    if len(out)+run>max_output:
                        raise DecodeError("decompression output exceeds image budget")
                    for _ in range(run):
                        out.append(out[-distance])
                else:raise DecodeError("invalid literal length token")
                if len(out)>max_output:
                    raise DecodeError("decompression output exceeds image budget")
        else:raise DecodeError("reserved DEFLATE block type")
        if len(out)>max_output:raise DecodeError("output limit exceeded")
        bits.align4()
        if final:break
    return bytes(out),blocks,bits.pos//8

def unfilter_scanlines(filtered,width,height,components,rows_per_block):
    stride=width*components
    expected=height*(stride+1)
    if len(filtered)!=expected:raise DecodeError(f"unpacked bytes {len(filtered)} != {expected}")
    out=bytearray(width*height*components)
    filter_counts={}
    for row in range(height):
        base=row*(stride+1);typ=filtered[base]
        if typ>4:raise DecodeError(f"unsupported PNG filter {typ} at row {row}")
        filter_counts[typ]=filter_counts.get(typ,0)+1
        boundary=(row%rows_per_block)==0
        for col in range(stride):
            left=out[row*stride+col-components] if col>=components else 0
            above=0 if boundary else out[(row-1)*stride+col]
            corner=0 if boundary or col<components else out[(row-1)*stride+col-components]
            if typ==0:predictor=0
            elif typ==1:predictor=left
            elif typ==2:predictor=above
            elif typ==3:predictor=(left+above)//2
            else:
                p=left+above-corner
                scores=(abs(p-left),abs(p-above),abs(p-corner))
                predictor=(left,above,corner)[scores.index(min(scores))]
            out[row*stride+col]=(filtered[base+1+col]+predictor)&255
    return bytes(out),filter_counts

def bgr_to_rgb(data):
    if len(data)%3:raise DecodeError("BGR888 byte count not divisible by 3")
    result=bytearray(len(data))
    result[0::3]=data[2::3]
    result[1::3]=data[1::3]
    result[2::3]=data[0::3]
    return bytes(result)

def png_chunk(kind,payload):
    return (struct.pack(">I",len(payload))+kind+payload
            +struct.pack(">I",zlib.crc32(kind+payload)&0xffffffff))

def write_rgb_png(path,pixels,width,height):
    if len(pixels)!=width*height*3:raise DecodeError("wrong RGB pixel length")
    stride=width*3
    data=b"".join(b"\x00"+pixels[i:i+stride] for i in range(0,len(pixels),stride))
    png=(b"\x89PNG\r\n\x1a\n"+
         png_chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,2,0,0,0))+
         png_chunk(b"IDAT",zlib.compress(data,9))+png_chunk(b"IEND",b""))
    Path(path).parent.mkdir(parents=True,exist_ok=True)
    Path(path).write_bytes(png)
    return hashlib.sha256(png).hexdigest()

def decode_file(src,out_png):
    blob=Path(src).read_bytes()
    layout=parse(blob,exact_monolith=False)
    if layout["lvgl_format"]!=1 or layout["bit_depth"]!=8:
        raise DecodeError("only legacy SiFli RGB888 type1/bit-depth8 verified")
    w,h=layout["width"],layout["height"]
    z=blob[4:]
    start=layout["offset_table_end"]
    shared=layout["blocks"][0]["start"]-start
    expanded,blocks,read_bytes=decode_lz77(
       z[start:],max_output=h*(w*3+1),shared_header_end=shared)
    raw,filters=unfilter_scanlines(expanded,w,h,3,layout["rows_per_block"])
    rgb=bgr_to_rgb(raw)
    checksum=write_rgb_png(out_png,rgb,w,h)
    return {"status":"PIXELS_DECODED_FROM_EZIP","source_sha256":layout["source_sha256"],
        "image_format":"RGB888","width":w,"height":h,
        "shared_dictionary_bytes":shared,"compressed_block_count":blocks,
        "encoded_bytes_consumed":read_bytes,"decoded_bytes":len(rgb),
        "pixel_sha256":hashlib.sha256(rgb).hexdigest(),
        "png_sha256":checksum,"png_path":str(out_png),
        "scanline_filters":filters,"oem_compatibility":"UNVERIFIED",
        "physical_upload_permitted":False}

def decode_reference_png(path):
    data=Path(path).read_bytes()
    if data[:8]!=b"\x89PNG\r\n\x1a\n":raise DecodeError("reference is not PNG")
    i=8;idats=[];dims=None
    while i<len(data):
        if i+12>len(data):raise DecodeError("truncated PNG chunk")
        n=struct.unpack_from(">I",data,i)[0]
        kind=data[i+4:i+8];val=data[i+8:i+8+n]
        if i+n+12>len(data) or zlib.crc32(kind+val)&0xffffffff!=struct.unpack_from(">I",data,i+8+n)[0]:
            raise DecodeError("PNG CRC or size error")
        i+=n+12
        if kind==b"IHDR":
            w,h,depth,fmt=struct.unpack_from(">IIBB",val)
            if (depth,fmt)!=(8,2):raise DecodeError("expected RGB888 reference")
            dims=(w,h)
        elif kind==b"IDAT":idats.append(val)
        elif kind==b"IEND":break
    if dims is None:raise DecodeError("missing PNG geometry")
    # PNG stream has filtering across entire image, not eZIP row-block resets.
    w,h=dims;filtered=zlib.decompress(b"".join(idats))
    return unfilter_scanlines(filtered,w,h,3,h)[0],dims

def compare_rgb(decoded_png,reference_png):
    a,adims=decode_reference_png(decoded_png)
    b,bdims=decode_reference_png(reference_png)
    if adims!=bdims or len(a)!=len(b):raise DecodeError("comparison geometry mismatch")
    squares=0;absolute=0;largest=0;zeros=0
    for x,y in zip(a,b):
        d=abs(x-y);absolute+=d;squares+=d*d;largest=max(largest,d);zeros+=(d==0)
    mean=absolute/len(a);rmse=math.sqrt(squares/len(a))
    return {"width":adims[0],"height":adims[1],
      "compared_channels":len(a),"MAE":mean,"RMSE":rmse,"maximum_channel_error":largest,
      "exact_channels":zeros,"PSNR_dB":float("inf") if rmse==0 else 20*math.log10(255/rmse),
      "identical_pixels":a==b}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("ezip_bin")
    p.add_argument("--output",required=True)
    p.add_argument("--reference-png")
    p.add_argument("--report")
    args=p.parse_args()
    report=decode_file(args.ezip_bin,args.output)
    if args.reference_png:report["pixel_comparison"]=compare_rgb(args.output,args.reference_png)
    if args.report:
        Path(args.report).write_text(json.dumps(report,indent=2,allow_nan=True)+"\n",encoding="utf-8")
    print(json.dumps(report,indent=2,allow_nan=True))
if __name__=="__main__":main()
