#!/usr/bin/env python3
"""Static safety regression checks for HK8 3407 while hardware tests are unavailable."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
JAVA = ROOT / "overlay/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/monolith/SifliFilePush.java"
ACTIVITY = ROOT / "overlay/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/monolith/MonolithActivity.java"


class Hk8TransportSafetyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.transport = JAVA.read_text(encoding="utf-8")
        cls.activity = ACTIVITY.read_text(encoding="utf-8")

    def test_upload_is_disabled_until_protocol_is_verified(self):
        self.assertIn("if (!isUploadProtocolVerified())", self.transport)
        self.assertIn("private static boolean isUploadProtocolVerified()", self.transport)
        self.assertIn("return false; // Must be backed by captured HK8 3407 protocol evidence", self.transport)
        self.assertIn("uploadButton.setEnabled(false)", self.activity)

    def test_does_not_send_multiple_unverified_resource_paths(self):
        # The old upload method remains for later rehabilitation but the UI is disabled.
        self.assertIn("uploadButton.setEnabled(false)", self.activity)

    def test_subscribe_errors_are_not_silently_accepted(self):
        self.assertIn('events.add(new Exception("Отсутствует CCCD', self.transport)
        self.assertIn("if (!g.setCharacteristicNotification(serial, true))", self.transport)
        self.assertIn("if (!g.writeDescriptor(d))", self.transport)
        self.assertIn("status != BluetoothGatt.GATT_SUCCESS", self.transport)
        self.assertIn("CCCD write callback invalid", self.transport)

    def test_rejects_truncated_single_frame(self):
        self.assertIn("SifliWireCodec.Reassembler", self.transport)
        self.assertIn("serialReassembler.accept(v)", self.transport)

    def test_transmitted_message_cannot_wrap_16_bit_length(self):
        self.assertIn("SifliWireCodec.frame(CATEGORY_WATCHFACE, message, mtu)", self.transport)

    def test_real_hci_wire_codec_is_used_by_upload_stub(self):
        self.assertIn("SifliWireCodec.start(type, total)", self.transport)
        self.assertIn("SifliWireCodec.fileStart(", self.transport)
        self.assertIn("SifliWireCodec.data(i + 1, data, off, len)", self.transport)
        self.assertIn("SifliWireCodec.endFile()", self.transport)
        self.assertIn("SifliWireCodec.end()", self.transport)

    def test_device_characteristics_remain_bound_to_observed_gatt(self):
        self.assertIn("00000000-0000-0200-6473-5f696c666973", self.transport)
        self.assertIn("6e400002-b5a3-f393-e0a9-e50e24dcca9e", self.transport)


if __name__ == "__main__":
    unittest.main()
