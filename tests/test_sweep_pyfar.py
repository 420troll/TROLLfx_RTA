"""Independent pyfar oracle for the portable native deconvolution kernel.

Run after scripts/test-sweep-dsp.sh with requirements-sweep-reference.txt.
"""
import json
import os
import pathlib
import subprocess
import tempfile
import unittest

import numpy as np
import pyfar as pf
import soundfile as sf

ROOT = pathlib.Path(__file__).resolve().parents[1]
CLI = pathlib.Path(os.environ.get("TROLLFX_SWEEP_CLI", ROOT / "build/trollfx-sweep-cli"))


class TestPyfarReference(unittest.TestCase):
    def test_two_tap_fixture_matches_pyfar(self):
        fixture = ROOT / "build/sweep-fixture"
        subprocess.run(["python3", str(ROOT / "scripts/make-sweep-fixture.py"), "--output", str(fixture)], check=True, capture_output=True)
        expected = json.loads((fixture / "expected.json").read_text())
        reference, fs = sf.read(fixture / "reference.wav")
        capture, capture_fs = sf.read(fixture / "synthetic_capture.wav")
        self.assertEqual(fs, capture_fs)
        nfft = 1 << (len(reference) + len(capture) - 1).bit_length()
        pyfar_ir = np.asarray(pf.dsp.deconvolve(
            pf.Signal(capture, fs), pf.Signal(reference, fs),
            fft_length=nfft, frequency_range=(40, 3000)).time).squeeze()
        output = fixture / "native-result"
        subprocess.run([str(CLI), "analyze", str(fixture / "reference.wav"),
                        str(fixture / "synthetic_capture.wav"), str(output),
                        "0", "40", "3000"], check=True, capture_output=True, text=True)
        native_ir, native_fs = sf.read(output / "impulse.wav")
        self.assertEqual(native_fs, fs)
        self.assertEqual(int(np.argmax(np.abs(native_ir))), int(np.argmax(np.abs(pyfar_ir))))
        self.assertLess(abs(native_ir.max() - pyfar_ir.max()) / pyfar_ir.max(), 0.02)
        direct = expected["known_impulse"][0]["sample"]
        echo = expected["known_impulse"][1]["sample"]
        self.assertLessEqual(abs(int(np.argmax(np.abs(native_ir))) - direct), 1)
        self.assertEqual(echo-direct, expected["expected_reflection_separation_samples"])
        for center in (direct, echo):
            lo, hi = center - 5, center + 6
            # The implementations use different outside-band regularization;
            # compare the physically meaningful main-lobe neighborhood.
            np.testing.assert_allclose(native_ir[lo:hi], pyfar_ir[lo:hi], rtol=0.03, atol=0.002)


if __name__ == "__main__":
    unittest.main()
