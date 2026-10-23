"""Real-source regression: independent decode of original MONOLITH BIN with exact RGB pixels."""
import importlib.util
import hashlib
from pathlib import Path
import tempfile
import unittest
root=Path(__file__).resolve().parents[1]
module=root/"watchface"/"ezip"/"decode_shared_huffman.py"
spec=importlib.util.spec_from_file_location("monolith_decoder",module)
import sys
sys.path.insert(0,str(module.parent))
d=importlib.util.module_from_spec(spec)
spec.loader.exec_module(d)

EXPECTED_PIXEL_SHA="1b02602419be4dd7c8e9db64073df99621d6542fa0c1eb5bfec685d46d77a940"
BIN=root/"overlay"/"app"/"src"/"main"/"assets"/"monolith"/"clock_bg.bin"
DECODED=root/"watchface"/"ezip"/"fixtures"/"clock_bg_decoded_from_ezip.png"

class DecoderTest(unittest.TestCase):
    def test_original_exact_pixels(self):
        with tempfile.TemporaryDirectory() as dirname:
            dst=Path(dirname)/"output.png"
            result=d.decode_file(BIN,dst)
            self.assertEqual(result["status"],"PIXELS_DECODED_FROM_EZIP")
            self.assertEqual(result["pixel_sha256"],EXPECTED_PIXEL_SHA)
            self.assertEqual(result["decoded_bytes"],756600)
            self.assertEqual(result["compressed_block_count"],17)
            self.assertEqual(result["shared_dictionary_bytes"],120)
            self.assertEqual(result["width"],485)
            self.assertEqual(result["height"],520)
            self.assertEqual(result["scanline_filters"],{1:19,2:447,4:54})
            self.assertEqual(dst.read_bytes(),DECODED.read_bytes())
            self.assertFalse(result["physical_upload_permitted"])

    def test_reference_pixel_match(self):
        a,_=d.decode_reference_png(DECODED)
        self.assertEqual(hashlib.sha256(a).hexdigest(),EXPECTED_PIXEL_SHA)

    def test_truncated_and_corrupted_payload_rejected(self):
        with tempfile.TemporaryDirectory() as dirname:
            src=BIN.read_bytes()
            for i,mutant in enumerate((src[:-100],src[:100],
                                       src[:209]+b"\xff"*500+src[709:])):
                file=Path(dirname)/(str(i)+".bin")
                file.write_bytes(mutant)
                with self.assertRaises((ValueError,IndexError,OverflowError),
                                       msg=f"invalid case {i}"):
                    d.decode_file(file,Path(dirname)/f"{i}.png")

    def test_standard_crc_and_pixel_converters(self):
        self.assertEqual(d.bgr_to_rgb(bytes([0,0,255,0,255,0,255,0,0])),
                         bytes([255,0,0,0,255,0,0,0,255]))
        with self.assertRaises(d.DecodeError):d.bgr_to_rgb(bytes([1,2]))
        with self.assertRaises(d.DecodeError):
            d.unfilter_scanlines(bytes([7,0,0,0]),1,1,3,1)
        with self.assertRaises(d.DecodeError):
            d.decode_lz77(b"\0",100)

if __name__=="__main__":unittest.main()
