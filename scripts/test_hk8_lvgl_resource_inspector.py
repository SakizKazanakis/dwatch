"""Public synthetic checks; private OEM executable is optional and never committed."""
import importlib.util
import os
from pathlib import Path
import struct
import tempfile
import unittest

p=Path(__file__).with_name("hk8_lvgl_resource_inspector.py")
spec=importlib.util.spec_from_file_location("resource_probe",p)
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

class ResourceInspectionTest(unittest.TestCase):
    def test_read_synthetic_u32_descriptor(self):
        b=b"xxx"+struct.pack("<III",6,0,0x8bf)
        self.assertEqual(mod.words_at(b,{".data":(0xaf0,3,12)},0xaf0,3),[6,0,0x8bf])

    def test_reject_unmapped_descriptor(self):
        with self.assertRaises(ValueError):
            mod.words_at(b"\0"*16,{".data":(0xaf0,0,12)},0xafc,3)

    def test_reject_mismatched_elf(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(b"not the OEM sample");f.flush()
            with self.assertRaises(ValueError):mod.inspect(f.name)
        with self.assertRaises(ValueError):mod.sections(b"not ELF")

    @unittest.skipUnless(os.environ.get("MONOLITH_OEM_FACE_ELF"),
                         "OEM binary intentionally not shipped to public CI")
    def test_private_original_fixture(self):
        result=mod.inspect(os.environ["MONOLITH_OEM_FACE_ELF"])
        self.assertEqual(result["image"]["raw_words"],["0x6","0x0","0x8bf"])
        self.assertEqual(len(result["linked_records"]),3)
        self.assertEqual(result["linked_records"][0]["packed_u32"],"0x7e000f")

if __name__=="__main__":
    unittest.main()
