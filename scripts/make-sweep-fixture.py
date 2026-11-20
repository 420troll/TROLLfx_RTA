#!/usr/bin/env python3
"""Deterministic synthetic sweep/reference fixture (standard library only).
Not a real acoustic recording; use it to validate offline C/C++ deconvolution.
"""
import argparse
import json
import math
import pathlib
import struct
import wave

def generate_sweep(fs=8000, duration=1.0, f0=40.0, f1=3000.0, level_dbfs=-30.0):
    if not (fs >= 8000 and 0 < f0 < f1 < .45*fs and 0 < duration <= 30 and -60 <= level_dbfs <= -6):
        raise ValueError("invalid sweep settings")
    n = round(fs * duration)
    L = duration / math.log(f1 / f0)
    gain = 10**(level_dbfs / 20)
    fade = max(1, round(.01 * fs))
    return [gain * max(0.0, min(1.0, i/fade, (n-1-i)/fade)) *
            math.sin(2*math.pi*f0*L*math.expm1(i/fs/L)) for i in range(n)]

def write_wav(path, values, fs):
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as fp:
        fp.setnchannels(1); fp.setsampwidth(2); fp.setframerate(fs)
        fp.writeframes(b"".join(struct.pack("<h", round(max(-1, min(1, x))*32767)) for x in values))

def fixture(path, fs=8000, duration=1.0):
    path = pathlib.Path(path)
    f0, f1 = 40., min(3000., fs*.375)
    s = generate_sweep(fs, duration, f0, f1)
    lead, direct, echo, tail = [round(fs*t) for t in (.125, .012, .047, .100)]
    y = [0.] * (lead + len(s) + echo + tail)
    for i, x in enumerate(s):
        y[lead+direct+i] += .5*x
        y[lead+echo+i] += .125*x
    write_wav(path/"reference.wav", s, fs)
    write_wav(path/"synthetic_capture.wav", y, fs)
    expected = {
        "fixture_type": "synthetic_two_tap_response",
        "measured_hardware": False,
        "sample_rate_hz": fs, "sample_format": "mono_pcm16",
        "sweep": {"type": "exponential_sine", "start_hz": f0, "stop_hz": f1,
                  "duration_s": duration, "peak_target_dbfs": -30., "fade_s": .01},
        "known_impulse": [
            {"sample": lead+direct, "relative_gain": .5},
            {"sample": lead+echo, "relative_gain": .125}
        ],
        "expected_reflection_separation_samples": echo-direct,
        "sweep_clock_ratio": 1.,
        "limitations": "No drift, nonlinear distortion, acoustic noise or SPL calibration"
    }
    path.mkdir(parents=True, exist_ok=True)
    (path/"expected.json").write_text(json.dumps(expected, indent=2)+"\n")
    return expected

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="build/sweep-fixture")
    parser.add_argument("--sample-rate", type=int, default=8000)
    parser.add_argument("--seconds", type=float, default=1.0)
    args = parser.parse_args()
    print(json.dumps(fixture(args.output,args.sample_rate,args.seconds),indent=2))
