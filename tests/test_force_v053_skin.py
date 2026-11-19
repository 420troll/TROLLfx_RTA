"""Static layout/binding checks for the V0.5.3 Force results pages."""
import json
import re
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SKIN = ROOT / "plugins/TrollfxRta/skins"


def index(entry):
    return int(entry["handle remapping"]["map"][0]["value"].split()[-1])


def main():
    manifest = json.loads((ROOT / "plugins/TrollfxRta/plugin.json").read_text())
    assert (manifest["version"], manifest["uid"], manifest["numInputs"], manifest["numOutputs"]) == ("0.5.3", "TfRA", 2, 2)
    tabs = json.loads((SKIN / "TUI.json").read_text())["pageData"]["tabs"]
    names = [tab["tabName"] for tab in tabs]
    assert "SWEEP LED RESULTS" in names and "SWEEP GRAPH" in names
    led_tab = next(tab for tab in tabs if tab["tabName"] == "SWEEP LED RESULTS")
    graph_tab = next(tab for tab in tabs if tab["tabName"] == "SWEEP GRAPH")
    assert led_tab["fnKeyIndex"] == graph_tab["fnKeyIndex"] == 5
    assert (led_tab["fnKeySubIndex"], graph_tab["fnKeySubIndex"]) == (0, 1)
    assert max(tab["fnKeyIndex"] for tab in tabs) <= 5, "do not assume a seventh Force function key exists"
    led = led_tab["componentDefinition"]["componentsData"]
    graph = graph_tab["componentDefinition"]["componentsData"]
    assert not any(e["componentData"]["type"] == "Plotter" for e in graph), "unreliable Plotter path must not remain"
    assert len([e for e in led if e["componentData"]["name"].startswith("sweepLedBand")]) == 31
    band_handles = {index(e) for e in led if e["componentData"]["name"].startswith("sweepLedBand")}
    assert band_handles == set(range(124, 155))
    for e in led:
        name = e["componentData"]["name"]
        if name.startswith("sweepLedBand") or name.startswith("sweepLedPeak"):
            x, y, w, h = map(float, e["bounds"]["bounds"].split())
            assert y >= 48 and y + h <= 477, "LED stack extends outside the response area"
    fills = [e for e in graph if e["componentData"]["name"].startswith("graphFill")]
    peaks = [e for e in graph if e["componentData"]["name"].startswith("graphTracePoint")]
    assert len(fills) == len(peaks) == 31
    assert {index(e) for e in fills} == set(range(155, 186))
    assert {index(e) for e in peaks} == set(range(218, 249))
    for fill, peak in zip(fills, peaks):
        bx = list(map(float, fill["bounds"]["bounds"].split()))
        px = list(map(float, peak["bounds"]["bounds"].split()))
        assert bx[1] == px[1] and abs(bx[0] - px[0]) <= 1.0 and abs(bx[2] - px[2]) <= 1.0
        d = fill["componentData"]["data"]
        assert d["sliderType"] == "ColourBar" and d["direction"] == "Vertical"
        assert fill["bounds"]["acceptsHWFocus"] == "No"
        assert px[0] + px[2] < 1025, "graph overlays the hardware Q-Link area"
    for page in (led, graph):
        handles = {index(e) for e in page if e["componentData"]["name"].endswith("ResultControl122") or e["componentData"]["name"].endswith("ResultControl123")}
        assert handles == {122, 123}, "both pages must expose smoothing and measurement selection"
        controls = [e for e in page if e["componentData"]["name"].endswith("ResultControl122") or e["componentData"]["name"].endswith("ResultControl123")]
        for control in controls:
            _, y, _, h = map(float, control["bounds"]["bounds"].split())
            assert h >= 40 and y + h <= 628, "touch selection controls must remain large and inside tab bounds"
        assert any(index(e) == 217 for e in page if e["componentData"]["name"].endswith("ResultStatus"))
        assert any(index(e) == 249 for e in page if "Diagnostic" in e["componentData"]["name"])
    for filename in ("sweep-led-panel.png", "sweep-graph-panel.png"):
        image = Image.open(SKIN / filename)
        assert image.size == (1280, 628)
    # Existing parameter order through V0.5.2 stays fixed; new parameters append.
    source = (ROOT / "plugins/TrollfxRta/src/TrollfxRta.cpp").read_text()
    assert re.search(r"kSweepResultStatus=kSweepRawDb0\+31", source)
    assert re.search(r"kSweepGraphPeak0=kSweepResultStatus\+1", source)
    assert re.search(r"kSweepDiagnostic=kSweepGraphPeak0\+31", source)
    print("PASS V0.5.3 skin: full-height 31-band LED and parameter-bound filled log response graph; legacy Plotter removed")


if __name__ == "__main__":
    main()
