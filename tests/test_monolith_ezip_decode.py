"""Reproducible exact-source eZIP decoder verification, no Pillow dependency."""
import importlib.util
import math
import struct
import zlib
from pathlib import Path
import unittest

HERE=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("decoder",HERE/"watchface/ezip/monolith_decoder.py")
d=importlib.util.module_from_spec(spec)
spec.loader.exec_module(d)
ASSET=HERE/"overlay/app/src/main/assets/monolith/clock_bg.bin"

def read_png_rgb(path):
    b=Path(path).read_bytes()
    if not b.startswith(b"\x89PNG\r\n\x1a\n"):raise ValueError("not PNG")
    pos=8; data=bytearray();w=h=None
    while pos<len(b):
        n,=struct.unpack_from(">I",b,pos);tag=b[pos+4:pos+8];raw=b[pos+8:pos+8+n]
        if zlib.crc32(tag+raw)&0xffffffff != struct.unpack_from(">I",b,pos+8+n)[0]:raise ValueError("bad PNG CRC")
        pos+=12+n
        if tag==b"IHDR":
            w,h,depth,fmt,*rest=struct.unpack(">IIBBBBB",raw)
            if (depth,fmt)!=(8,2):raise ValueError("unsupported PNG")
        if tag==b"IDAT":data.extend(raw)
        if tag==b"IEND":break
    packed=zlib.decompress(data)
    stride=w*3;out=bytearray(w*h*3)
    for y in range(h):
        kind=packed[y*(stride+1)]
        for x in range(stride):
            a=out[y*stride+x-3] if x>=3 else 0
            up=out[(y-1)*stride+x] if y else 0
            ul=out[(y-1)*stride+x-3] if y and x>=3 else 0
            if kind==0:pred=0
            elif kind==1:pred=a
            elif kind==2:pred=up
            elif kind==3:pred=(a+up)//2
            elif kind==4:
                p=a+up-ul;da,db,dc=abs(p-a),abs(p-up),abs(p-ul)
                pred=a if da<=db and da<=dc else up if db<=dc else ul
            else:raise ValueError("invalid PNG filter")
            out[y*stride+x]=(packed[y*(stride+1)+1+x]+pred)&255
    return w,h,bytes(out)

class EzipTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rgb,cls.meta=d.decode(ASSET.read_bytes())
    def test_real_data_recovered(self):
        self.assertEqual(len(self.rgb),756600)
        self.assertEqual(self.meta["expanded_bytes"],757120)
        self.assertEqual(self.meta["rgb_sha256"],"1b02602419be4dd7c8e9db64073df99621d6542fa0c1eb5bfec685d46d77a940")
    def test_original_png_lossless(self):
        # The actual source PNG is in a historical branch, supplied via env
        import os
        path=os.environ.get("MONOLITH_ORIGINAL_PNG")
        if not path:self.skipTest("original PNG from historical branch not included in this branch")
        w,h,expected=read_png_rgb(path)
        self.assertEqual((w,h),(485,520))
        self.assertEqual(expected,self.rgb)
    def test_generated_png_roundtrip(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"decoded.png";p.write_bytes(d.png_encode(self.rgb))
            w,h,rgb=read_png_rgb(p)
            self.assertEqual((w,h),(485,520))
            self.assertEqual(rgb,self.rgb)
    def test_invalid_input_rejected(self):
        raw=ASSET.read_bytes()
        for changed in [raw[:80],raw[:4]+bytes([x^1 for x in raw[4:5]])+raw[5:],
                        b"\0"*len(raw)]:
            with self.assertRaises((d.FormatError,struct.error)):
                d.decode(changed)
    def test_expansion_guard(self):
        with self.assertRaises(d.FormatError):d.legacy_shared_huffman(b"\xff"*40)
if __name__=="__main__":unittest.main()
