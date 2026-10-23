#!/usr/bin/env python3
"""Static guardrails for the HK8 read-only diagnostic (not hardware tests)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "overlay/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/monolith"
ACTIVITY = BASE / "MonolithActivity.java"
PROBE = BASE / "MonolithBleProbe.java"
PROFILE = BASE / "MonolithBleProfile.java"
LAYOUT = ROOT / "overlay/app/src/main/res/layout/monolith_activity.xml"


class Hk8ReadOnlyProbeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe = PROBE.read_text(encoding="utf-8")
        cls.profile = PROFILE.read_text(encoding="utf-8")
        cls.activity = ACTIVITY.read_text(encoding="utf-8")
        cls.layout = LAYOUT.read_text(encoding="utf-8")

    def test_no_gatt_writes_or_subscriptions(self):
        self.assertNotIn(".writeCharacteristic(", self.probe)
        self.assertNotIn(".writeDescriptor(", self.probe)
        self.assertNotIn(".setCharacteristicNotification(", self.probe)
        self.assertNotIn(".readCharacteristic(", self.probe)
        self.assertNotIn(".requestMtu(", self.probe)

    def test_read_only_discovery_and_cleanup(self):
        self.assertIn(".connectGatt(", self.probe)
        self.assertIn(".discoverServices()", self.probe)
        self.assertIn(".getServices()", self.probe)
        self.assertIn(".close()", self.probe)
        self.assertIn("CountDownLatch", self.probe)

    def test_observed_hk8_uuid_and_profile_verdicts(self):
        self.assertIn("00000000-0000-0200-6473-5f696c666973", self.profile)
        self.assertIn("6e400002-b5a3-f393-e0a9-e50e24dcca9e", self.profile)
        self.assertIn("AVAILABLE", self.profile)
        self.assertIn("NOT FOUND", self.profile)
        self.assertIn("not verified", self.profile)

    def test_export_is_explicit_user_action(self):
        self.assertIn("Intent.ACTION_SEND", self.activity)
        self.assertIn("text/plain", self.activity)
        self.assertIn("monolith_ble_share", self.layout)
        self.assertIn("monolith_ble_diagnose", self.layout)

    def test_dial_upload_stays_disabled(self):
        self.assertIn("uploadButton.setEnabled(false)", self.activity)
        self.assertNotIn("uploadButton.setOnClickListener", self.activity)

    def test_never_claims_to_capture_wearfit_traffic(self):
        self.assertIn("only this GATT client", self.probe)


if __name__ == "__main__":
    unittest.main()
