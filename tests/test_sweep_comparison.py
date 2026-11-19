import csv
import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("sweep_compare", ROOT / "scripts/compare-sweep.py")
compare = importlib.util.module_from_spec(spec); spec.loader.exec_module(compare)


class TestSweepComparison(unittest.TestCase):
    def test_saved_responses_are_compared_on_log_frequency_grid(self):
        with tempfile.TemporaryDirectory() as td:
            td = pathlib.Path(td)
            for name, gain in (("a", 0), ("b", 3)):
                with (td / f"{name}.csv").open("w") as fp:
                    fp.write("frequency_hz,magnitude,gain_db,phase_rad\n")
                    for f in (20, 100, 1000, 10000, 20000): fp.write(f"{f},1,{gain},0.2\n")
            compare.compare(td / "a.csv", td / "b.csv", td / "out")
            with (td / "out/comparison.csv").open() as fp: rows = list(csv.DictReader(fp))
            self.assertEqual(len(rows), 512)
            self.assertTrue(all(abs(float(r["B_minus_A_db"]) - 3) < 1e-9 for r in rows))
            self.assertTrue((td / "out/comparison.svg").exists())


if __name__ == "__main__": unittest.main()
