"""Independent NumPy quadrature check of the C++ log-frequency smoother."""
import csv
import math
import os
import pathlib
import subprocess
import tempfile
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
CLI = pathlib.Path(os.environ.get("TROLLFX_SWEEP_CLI", ROOT / "build/trollfx-sweep-cli"))


class TestSmoothingReference(unittest.TestCase):
    def test_cpp_matches_dense_numpy_log_quadrature(self):
        with tempfile.TemporaryDirectory() as td:
            folder = pathlib.Path(td)
            frequency = np.geomspace(20.0, 20000.0, 1501)
            log2 = np.log2(frequency / 1000.0)
            magnitude = 0.35 + 0.8 * np.exp(-0.5 * (log2 / 0.04) ** 2) + 0.12 * np.log10(frequency / 20.0) / 3.0
            with (folder / "frequency-response.csv").open("w", newline="") as fp:
                writer = csv.writer(fp, lineterminator="\n"); writer.writerow(("frequency_hz", "magnitude", "gain_db", "phase_rad"))
                writer.writerows(zip(frequency, magnitude, 20 * np.log10(magnitude), np.zeros_like(frequency)))
            (folder / "measurement.json").write_text('{"format":"V0.5"}\n')
            subprocess.run([str(CLI), "reprocess", str(folder), "1/6"], check=True, capture_output=True)
            with (folder / "frequency-response-smoothed.csv").open() as fp:
                actual = list(csv.DictReader(fp))
            for idx in (400, 700, 900, 1100):
                fc = frequency[idx]; lo = fc * 2 ** (-1 / 12); hi = fc * 2 ** (1 / 12)
                # Dense independent quadrature over ln(f), interpolating source
                # magnitude from the original measured coordinates.
                grid = np.linspace(math.log(max(lo, frequency[0])), math.log(min(hi, frequency[-1])), 10001)
                values = np.interp(grid, np.log(frequency), magnitude)
                expected = np.sum((values[:-1] + values[1:]) * np.diff(grid) * 0.5) / (grid[-1] - grid[0])
                self.assertAlmostEqual(float(actual[idx]["magnitude"]), expected, delta=2e-6)


if __name__ == "__main__":
    unittest.main()
