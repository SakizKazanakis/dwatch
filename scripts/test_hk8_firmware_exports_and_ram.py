"""Offline-only exact ELF symbol reading and calibrated memory arithmetic."""
import importlib.util
import pathlib
import subprocess
import shutil
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).with_name("hk8_firmware_exports_and_ram.py")
spec = importlib.util.spec_from_file_location("hk8memory",SCRIPT)
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)

class MemoryAudit(unittest.TestCase):
    def test_unmeasured_is_never_fabricated(self):
        m=api.memory_requirements()
        self.assertEqual(m["stream_decoder_heap_bytes"],37133)
        self.assertEqual(m["target_framebuffer_rgb565_bytes"],405080)
        self.assertEqual(m["target_framebuffer_argb8888_bytes"],810160)
        self.assertEqual(m["monolith_image_lvgl32_bytes"],1008800)
        self.assertEqual(m["monolith_image_lvgl16_bytes"],504400)
        self.assertEqual(m["sf32lb55_family_sram_hcpu_bytes"],1114112)
        self.assertEqual(m["sf32lb55_family_sram_lcpu_bytes"],229376)
        self.assertIsNone(m["device_psram_total_bytes"])
        self.assertIsNone(m["device_hcpu_heap_free_bytes"])
        self.assertEqual(m["actual_available_memory_status"],"UNMEASURED")
    def test_captured_shell_free_is_scope_limited(self):
        txt="msh />free\n total memory: 7669836\n used memory : 15240\n maximum allocated memory: 18520\n"
        r=api.parse_free_output(txt)
        self.assertEqual(r["reported_heap_free_arithmetic_bytes"],7654596)
        self.assertIsNone(r["psram_total_bytes"])
        self.assertIsNone(r["largest_free_allocation_bytes"])
        with self.assertRaises(ValueError):api.parse_free_output("free: failed")
        with self.assertRaises(ValueError):
            api.parse_free_output("total memory: 20\nused memory: 40\nmaximum allocated memory: 10")
    def test_detect_real_arm_elf_imports_without_inventing_rom(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp)
            src=root/"stub.c"
            src.write_text("""
              extern void wf_obj_img_create(void);
              extern void lv_cont_create(void);
              void probe(void) { wf_obj_img_create(); lv_cont_create(); }
            """)
            elf=root/"probe.so"
            obj=root/"probe.o"
            clang=shutil.which("clang") or shutil.which("clang-18") or "clang"
            linker=shutil.which("ld.lld") or shutil.which("ld.lld-18") or "ld.lld"
            subprocess.run([clang,"--target=armv7a-none-eabihf","-fPIC",
                            "-c",str(src),"-o",str(obj)],
                           check=True,capture_output=True,text=True)
            subprocess.run([linker,"-shared",str(obj),"-o",str(elf)],
                           check=True,capture_output=True,text=True)
            out=api.inspect(elf)
            self.assertEqual(out["required_external_import_count"],2)
            self.assertEqual(out["requires_firmware_or_other_modules"],
                             ["lv_cont_create","wf_obj_img_create"])
            self.assertIsNone(out["full_firmware_export_list"])
            self.assertFalse(out["matched_device_firmware_proven"])
            self.assertIsNone(out["verified_firmware_export_coverage"])
            bad=root/"not_an_elf.bin"
            bad.write_bytes(b"mock firmware contains lv_cont_create but no ELF")
            with self.assertRaises(ValueError):api.read_elf_symbols(bad)

if __name__=="__main__":
    unittest.main()
