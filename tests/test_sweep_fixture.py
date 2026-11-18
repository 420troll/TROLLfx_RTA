#!/usr/bin/env python3
"""Run: python3 -m unittest discover -s tests -p test_sweep_fixture.py"""
import json
import pathlib
import sys
import tempfile
import unittest
import wave
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/"scripts"))
from importlib.machinery import SourceFileLoader
fixture_module = SourceFileLoader("sweep_fixture",str(pathlib.Path(__file__).resolve().parents[1]/"scripts"/"make-sweep-fixture.py")).load_module()

class TestSweepFixture(unittest.TestCase):
    def test_safe_sweep(self):
        x=fixture_module.generate_sweep()
        self.assertEqual(len(x),8000)
        self.assertLessEqual(max(abs(v) for v in x),10**(-30/20)+1e-12)
        self.assertEqual(x[0],0)
        self.assertEqual(x[-1],0)
        with self.assertRaises(ValueError):
            fixture_module.generate_sweep(f1=4000)

    def test_synthetic_two_tap(self):
        with tempfile.TemporaryDirectory() as tmp:
            expected=fixture_module.fixture(tmp)
            d=json.loads((pathlib.Path(tmp)/"expected.json").read_text())
            self.assertFalse(d["measured_hardware"])
            self.assertEqual(d["known_impulse"],expected["known_impulse"])
            self.assertEqual(d["expected_reflection_separation_samples"],280)
            with wave.open(str(pathlib.Path(tmp)/"reference.wav")) as wav:
                self.assertEqual(wav.getnframes(),8000)
                self.assertEqual(wav.getframerate(),8000)
            with wave.open(str(pathlib.Path(tmp)/"synthetic_capture.wav")) as wav:
                self.assertGreater(wav.getnframes(),8000)

if __name__=="__main__":
    unittest.main()
