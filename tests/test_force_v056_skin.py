"""V0.5.6 skin integrity: full-scale LED view and connected sweep curve."""
import json
import re
import struct
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SKIN = ROOT / "plugins/TrollfxRta/skins"


def handle(e):
    maps = e["handle remapping"]["map"]
    assert len(maps) == 1
    return int(maps[0]["value"].rsplit(" ", 1)[1])


def entries(tabs, name):
    return next(t for t in tabs if t["tabName"] == name)["componentDefinition"]["componentsData"]


def main():
    manifest = json.loads((ROOT / "plugins/TrollfxRta/plugin.json").read_text())
    assert (manifest["name"], manifest["version"], manifest["uid"], manifest["numInputs"], manifest["numOutputs"]) == (
        "TROLLfx - VST - RTA", "0.5.6", "TfRA", 2, 2)
    tabs = json.loads((SKIN / "TUI.json").read_text())["pageData"]["tabs"]
    led = entries(tabs, "SWEEP LED RESULTS")
    graph = entries(tabs, "SWEEP GRAPH")
    sweep = entries(tabs, "SWEEP")
    saved = entries(tabs, "SAVED SWEEPS")

    bands = [e for e in led if e["componentData"]["name"].startswith("sweepLedBand")]
    assert len(bands) == 31 and {handle(e) for e in bands} == set(range(124, 155))
    assert not any(e["componentData"]["name"].startswith(("sweepLedPeak", "sweepLedClip")) for e in led)
    for e in bands:
        x, y, w, h = map(float, e["bounds"]["bounds"].split())
        assert x >= 95 and y >= 64 and x + w <= 1257 and y + h <= 548
    assert {handle(e) for e in led if "DbTick" in e["componentData"]["name"]} == set(range(254, 259))

    segments = [e for e in graph if e["componentData"]["name"].startswith("graphLineSegment")]
    assert len(segments) == 255 and {handle(e) for e in segments} == set(range(259, 514))
    assert all(e["componentData"]["type"] == "Image" for e in segments)
    assert all(e["componentData"]["data"]["imageType"] == "FilmStrip" for e in segments)
    assert all(e["componentData"]["data"]["numFrames"] == 962 for e in segments)
    assert all(e["componentData"]["data"]["image"] == "sweep-graph-segments.png" for e in segments)
    assert all(e["bounds"]["acceptsHWFocus"] == "No" for e in segments)
    for e in segments:
        x, y, w, h = map(float, e["bounds"]["bounds"].split())
        assert x >= 95 and y == 64 and x + w <= 1257.1 and y + h == 548
    assert not any(e["componentData"]["type"] == "Plotter" for e in graph)
    assert not any(e["componentData"]["name"].startswith(("graphClip", "graphRaw", "graphBar")) for e in graph)
    atlas = Image.open(SKIN / "sweep-graph-segments.png")
    assert atlas.mode == "RGBA" and atlas.size == (4, 64 * 962), atlas.size
    # Atlas includes rendered trace pixels and a fully transparent missing frame.
    assert atlas.crop((0, 64 * 961, 4, 64 * 962)).getbbox() is None
    assert atlas.crop((0, 0, 4, 64)).getbbox() is not None

    # Both result pages are full-screen, show actual dynamic bounds/ticks, and
    # contain no user controls. Legacy parameters remain in the plugin state.
    for page in (led, graph):
        names = [e["componentData"]["name"] for e in page]
        assert not any("Control" in n or "Slider" in n or "Knob" in n for n in names)
        assert not any(handle(e) in (122, 123, 250, 251) for e in page if e["handle remapping"]["map"])
        assert {handle(e) for e in page if "DbTick" in e["componentData"]["name"]} == set(range(254, 259))
        assert any(e["handle remapping"]["map"] and handle(e) == 217 for e in page)
    assert not any("smoothing" in e["componentData"]["name"].lower() for e in sweep + saved)
    assert len([e for e in saved if e["componentData"]["name"].startswith("sweep-slot-")]) == 8

    qlinks = json.loads((SKIN / "Q-Links.json").read_text())["Screen Mode Q-Links"]["map"]
    assert all(122 not in q["Q-Links"].values() and 250 not in q["Q-Links"].values() and
               251 not in q["Q-Links"].values() for q in qlinks)
    result_maps = {(q["Tab"], q["SubTab"]): q["Q-Links"] for q in qlinks}
    assert result_maps[(6, 1)]["Q-Link 1"] == 123
    assert result_maps[(6, 2)]["Q-Link 1"] == 123
    assert result_maps[(6, 1)]["Q-Link 2"] == result_maps[(6, 2)]["Q-Link 2"] == -1
    for filename in ("sweep-led-panel.png", "sweep-graph-panel.png"):
        assert Image.open(SKIN / filename).size == (1280, 628)

    source = (ROOT / "plugins/TrollfxRta/src/TrollfxRta.cpp").read_text()
    assert "kSweepGraphSegmentCount=255" in source
    assert "kSweepDisplaySmoothingN" in source
    assert re.search(r"kSweepSmoothing,", source)  # preserved stable legacy parameter index
    assert 'index == kSweepMeasurement) { displayRefreshRequested_' in source
    assert 'index == kSweepSmoothing' not in source
    generator = (ROOT / "scripts/generate-force-rta-skin.py").read_text()
    assert "GRAPH_STEPS = 31" in generator and "fill=(255,46,55,255)" in generator
    assert "def clipping_strip" not in generator
    print("PASS V0.5.6 skin: 31 full-height LED bands, auto-range ticks, fixed smoothing, clean result pages, Q-Link saved-measurement selection and 255 continuous line segments")


if __name__ == "__main__":
    main()
