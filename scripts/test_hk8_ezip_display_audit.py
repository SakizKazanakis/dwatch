"""Offline tests for source-preserving SiFli EZIP/OEM memory audit."""
import importlib.util
import os
import struct
import tempfile
import unittest
from pathlib import Path

path=Path(__file__).with_name("hk8_ezip_display_audit.py")
spec=importlib.util.spec_from_file_location("audit",path)
audit=importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

class Hk8DisplayTest(unittest.TestCase):
    def test_ezip_headers(self):
        original=audit.image_header(bytes.fromhex("0268c63d")+b"\x00"*8)
        monolith=audit.image_header(bytes.fromhex("01940741")+b"\x00"*8)
        self.assertEqual((original["width"],original["height"],original["format"]),(410,494,2))
        self.assertEqual((monolith["width"],monolith["height"],monolith["format"]),(485,520,1))

    def test_mpeg_crc_reference(self):
        self.assertEqual(audit.crc32_mpeg2(b"123456789"),0x0376e6e7)

    def test_invalid_header_is_rejected(self):
        for d in [b"",b"\x00"*8,bytes.fromhex("01000000")+b"\0"*4]:
            with self.assertRaises(ValueError):audit.image_header(d)

    def test_image_geometry_conflict_is_explicit(self):
        report=audit.geometry(485,520,410,494)
        self.assertEqual(report["overflow"],[75,26])
        self.assertEqual(report["no_scale_center_crop"],[37,38,13,13])
        self.assertEqual(report["contain_size"],[410,440])
        self.assertIsNone(report["approved_transform"])

    def test_oem_rejects_unauthorized_elf(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"false.so"
            p.write_bytes(b"\x7fELF"+b"\x00"*4000)
            with self.assertRaises(ValueError):audit.inspect_elf(p)

    @unittest.skipUnless(os.environ.get("MONOLITH_OEM_FACE_ELF"),"Private OEM ELF not published to CI")
    def test_private_oem_thumb_fixture(self):
        d=audit.inspect_elf(Path(os.environ["MONOLITH_OEM_FACE_ELF"]))
        self.assertEqual(d["module_resource_manager_bytes"],44)
        self.assertEqual(d["view_user_data_request_bytes"],28)
        self.assertEqual(d["lvgl_root"],[410,494])
        self.assertEqual(d["opaque_vendor_image_helper"],"wf_obj_img_create")

if __name__=="__main__":unittest.main()
