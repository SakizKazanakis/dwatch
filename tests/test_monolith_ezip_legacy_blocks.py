"""Fail-closed eZIP parser test suite; fixture includes real original file in CI."""
import importlib.util
from pathlib import Path
import struct
import unittest
p=Path(__file__).parents[1]/"watchface"/"ezip"/"inspect_legacy_blocks.py"
spec=importlib.util.spec_from_file_location("ezipinspect",p)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class TestLegacyEzipInspector(unittest.TestCase):
    def test_synthetic_table(self):
        # 2x3 pixels, 2 rows per block => 2 compressed blocks
        w,h=2,3
        lv=(1|(w<<10)|(h<<21)).to_bytes(4,"little")
        n=2
        inner=struct.pack(">IBBBBHHBBBB",44,0x42,8,2,0,w,h,0,0,0,0)
        inner+=struct.pack(">HHII",2,n,36,40)+b"ABCD"+b"EFGH"
        self.assertEqual(len(inner),44)
        result=m.parse(lv+inner)
        self.assertEqual(result["block_count"],2)
        self.assertEqual(result["blocks"][0]["bytes"],4)
        self.assertEqual(result["status"],"BLOCK_TABLE_VERIFIED__PIXELS_NOT_DECODED")

    def test_reject_modified_content_if_exact_required(self):
        b=b"\x01"*100
        with self.assertRaises(m.EzipFormatError):m.parse(b,True)

    def test_reject_count_and_offsets(self):
        w,h=2,3
        lv=(1|(w<<10)|(h<<21)).to_bytes(4,"little")
        inner=struct.pack(">IBBBBHHBBBB",44,0x42,8,2,0,w,h,0,0,0,0)
        for rows,n,offsets in [(2,3,[36,40]),(2,2,[40,36]),(2,2,[5,40])]:
            payload=inner+struct.pack(">HHII",rows,n,*offsets)+b"ABCDEFGH"
            with self.assertRaises(m.EzipFormatError):m.parse(lv+payload)

if __name__=="__main__":unittest.main()
