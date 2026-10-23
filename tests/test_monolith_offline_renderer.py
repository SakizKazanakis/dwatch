"""Tests for exact-source offline Monolith renderer. No Pillow/network/hardware."""
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

p=Path(__file__).resolve().parents[1]/"watchface"/"renderer"/"build_offline_renderer.py"
spec=importlib.util.spec_from_file_location("offline_renderer",p)
r=importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)

class RendererTests(unittest.TestCase):
    @staticmethod
    def synthetic_png():
        import zlib
        def chunk(kind,body):
            return struct.pack(">I",len(body))+kind+body+struct.pack(">I",zlib.crc32(kind+body))
        return b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",485,520,8,2,0,0,0))+chunk(b"IEND",b"")
    def test_dimensions_and_reject_modification(self):
        b=self.synthetic_png()
        self.assertEqual((r.png_info(b)["width"],r.png_info(b)["height"]),(485,520))
        with self.assertRaises(ValueError):r.png_info(b"\x00"*100)
        bad=bytearray(b);bad[16:20]=struct.pack(">I",410)
        with self.assertRaises(ValueError):r.png_info(bad)
    def test_ezip_rejects_wrong_hash_even_if_header_matches(self):
        f=(0x41079401).to_bytes(4,"little")+b"\x00"*(158937-4)
        with self.assertRaises(ValueError):r.ezip_info(f)
    def test_build_embeds_original_bytes_and_does_not_claim_firmware(self):
        b=self.synthetic_png()
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/"bg.png";src.write_bytes(b)
            dst=Path(d)/"watchface.html"
            report=r.build(src,None,dst)
            text=dst.read_text()
            self.assertIn("data:image/png;base64,",text)
            self.assertIn("NOT VERIFIED",text)
            self.assertIn('value="contain"',text)
            self.assertIn('value="crop"',text)
            self.assertIn("setInterval(render,1000)",text)
            self.assertEqual(report["verified"]["png"]["width"],485)

if __name__=="__main__":unittest.main()
