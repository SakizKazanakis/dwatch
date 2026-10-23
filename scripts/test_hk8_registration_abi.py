#!/usr/bin/env python3
"""Static ABI evidence regression tests (OEM ELF is intentionally not committed)."""
import importlib.util
import os
from pathlib import Path
import unittest

PROBE=Path(__file__).with_name("hk8_watchface_registration_probe.py")
spec=importlib.util.spec_from_file_location("hk8_abi_probe",PROBE)
probe=importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class Hk8RegistrationAbiTest(unittest.TestCase):
    def test_thumb_pc_relative_got(self):
        self.assertEqual(probe.thumb_pc_relative_add(0x7da,0x2a2),0xa80)

    def test_thumb_ui_callback_address(self):
        self.assertEqual(probe.thumb_pc_relative_add(0x7e8,0xfffffe7d),0x669)

    def test_thumbnail_address(self):
        self.assertEqual(probe.thumb_pc_relative_add(0x7ea,0x1d4),0x9c2)

    def test_name_address(self):
        self.assertEqual(probe.thumb_pc_relative_add(0x7ec,0xc4),0x8b4)

    def test_callback_event_table(self):
        self.assertEqual(probe.tbb_destinations(0x67c,bytes([2,0x52,0x63,0x72])),
                         [0x680,0x720,0x742,0x760])

    def test_rejects_invalid_elf(self):
        with self.assertRaises(ValueError):
            probe.elf_sections(b"not ELF")

    @unittest.skipUnless(os.environ.get("MONOLITH_OEM_FACE_ELF"),
                         "Private OEM module is not present in public CI")
    def test_optional_exact_private_capture(self):
        result=probe.inspect(os.environ["MONOLITH_OEM_FACE_ELF"])
        reg=result["registration"]
        self.assertEqual(reg["callee"],"app_tileview_register_watchface")
        self.assertEqual(reg["r0_name"],"wf_clock23")
        self.assertEqual(reg["r1_integer"],0)
        self.assertEqual(reg["r2_thumbnail"],"/ex/installer_wf/wf_clock23_tn.bin")
        self.assertEqual(reg["r3_thumb_handler"],"0x669")
        self.assertEqual(result["init_rt_malloc_bytes"],44)
        self.assertEqual(result["ui_event0"]["width"],410)
        self.assertEqual(result["ui_event0"]["height"],494)
        self.assertEqual(result["init_calls"],[
            "rt_malloc","dlmodule_set_res_mng",
            "lv_ext_res_mng_builtin_init","lv_ext_get_locale",
            "lv_ext_set_locale","wf_clock23_register"])
        self.assertEqual(result["ui_static_records"]["record_0xaf0"],
                         ["0x6","0x0","0x8bf"])
        self.assertEqual(result["ui_static_records"]["record_0xaf0_path"],
                         "/ex/resource/wf_clock23/wf_clock23_bg.bin")
        self.assertEqual(result["ui_static_records"]["record_0xafc"],
                         ["0xcd0005","0xf7","0x9ec"])
        self.assertEqual(len(result["ui_static_records"]["linked_resource_paths"]),6)
        self.assertEqual(result["cleanup_calls"],[
            "dlmodule_get_res_mng","lv_ext_res_mng_builtin_destroy","rt_free"])


if __name__=="__main__":
    unittest.main()
