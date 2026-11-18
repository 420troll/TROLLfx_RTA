#!/usr/bin/env python3
"""Create a log-frequency A/B response comparison from saved CSV results."""
import argparse
import csv
import math
import pathlib


def read(path):
    with open(path, newline="") as fp:
        rows = [(float(r["frequency_hz"]), float(r["gain_db"]), float(r["phase_rad"]))
                for r in csv.DictReader(fp) if float(r["frequency_hz"]) > 0]
    rows.sort()
    if len(rows) < 2:
        raise ValueError(f"{path}: response needs at least two positive-frequency bins")
    return rows


def interpolate(rows, frequency, column):
    x = math.log(frequency)
    for a, b in zip(rows, rows[1:]):
        if a[0] <= frequency <= b[0]:
            t = (x - math.log(a[0])) / (math.log(b[0]) - math.log(a[0]))
            return a[column] + t * (b[column] - a[column])
    return None


def compare(path_a, path_b, output):
    a, b = read(path_a), read(path_b)
    low, high = max(a[0][0], b[0][0]), min(a[-1][0], b[-1][0])
    if low >= high:
        raise ValueError("responses have no common frequency range")
    # Log-spaced bins avoid over-weighting the upper octaves.
    count = 512
    rows = []
    for i in range(count):
        f = low * (high / low) ** (i / (count - 1))
        av, bv = interpolate(a, f, 1), interpolate(b, f, 1)
        ap, bp = interpolate(a, f, 2), interpolate(b, f, 2)
        rows.append((f, av, bv, bv - av, bp - ap))
    out = pathlib.Path(output)
    out.mkdir(parents=True, exist_ok=True)
    with (out / "comparison.csv").open("w", newline="") as fp:
        w = csv.writer(fp); w.writerow(("frequency_hz", "A_gain_db", "B_gain_db", "B_minus_A_db", "phase_difference_rad")); w.writerows(rows)
    width, height, left, top, plotw, ploth = 1200, 660, 85, 55, 1080, 500
    ylo, yhi = -60, 30
    xcoord = lambda f: left + plotw * math.log(f / low) / math.log(high / low)
    ycoord = lambda v: top + ploth * (yhi - max(ylo, min(yhi, v))) / (yhi - ylo)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}"><rect width="100%" height="100%" fill="#101010"/>',
           '<text x="600" y="30" text-anchor="middle" fill="white" font-family="sans-serif" font-size="20">Sweep response comparison</text>']
    for db in range(ylo, yhi + 1, 10):
        y = ycoord(db); svg += [f'<path d="M{left} {y:.2f} H{left+plotw}" stroke="#444"/>', f'<text x="{left-8}" y="{y+4:.2f}" text-anchor="end" fill="#aaa" font-size="12">{db} dB</text>']
    for f in (20, 100, 1000, 10000, 20000):
        if low <= f <= high:
            x = xcoord(f); svg += [f'<path d="M{x:.2f} {top} V{top+ploth}" stroke="#333"/>', f'<text x="{x:.2f}" y="{top+ploth+22}" text-anchor="middle" fill="#aaa" font-size="12">{f:g} Hz</text>']
    for column, color in ((1, "#ff3030"), (2, "#40a0ff")):
        points = " ".join(("M" if i == 0 else "L") + f"{xcoord(row[0]):.2f},{ycoord(row[column]):.2f}" for i, row in enumerate(rows))
        svg.append(f'<path d="{points}" fill="none" stroke="{color}" stroke-width="2"/>')
    svg.append(f'<text x="600" y="{top+ploth+55}" text-anchor="middle" fill="#aaa" font-size="14">Frequency (log scale) · A red · B blue · difference in comparison.csv</text></svg>')
    (out / "comparison.svg").write_text("".join(svg))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("response_a"); parser.add_argument("response_b"); parser.add_argument("--output", default="comparison")
    args = parser.parse_args()
    compare(args.response_a, args.response_b, args.output)
