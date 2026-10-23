import tempfile
import unittest
import struct
from pathlib import Path
import importlib.util
p=Path(__file__).with_name("check_monolith_existing_dial.py")
s=importlib.util.spec_from_file_location("monolith_existing_dial",p)
m=importlib.util.module_from_spec(s)
s.loader.exec_module(m)
class ExistingDialTests(unittest.TestCase):
    def test_original_lvgl_header_dimensions(self):
        h=int.from_bytes(m.EXPECTED_HEADER,"little")
        self.assertEqual(((h>>10)&2047,(h>>21)&2047,h&31),(485,520,1))
    def test_rejects_fake_or_truncated_resource(self):
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/"bad.bin"
            f.write_bytes(m.EXPECTED_HEADER + b"\0"*7)
            with self.assertRaises(ValueError):m.inspect(f)
    def test_preserved_source_not_inflated_to_hardware_proof(self):
        self.assertNotEqual((m.EXPECTED_WIDTH,m.EXPECTED_HEIGHT),m.WATCHFACE_ROOT)
        self.assertEqual(m.HYPOTHETICAL_REMOTE,"/ex/resource/monolith/clock_bg.bin")
if __name__=="__main__":
    unittest.main()
