"""V1.0 Force skin compatibility and multi-layer rendering checks."""
import json
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
SKIN = ROOT / "plugins/TrollfxRta/skins"
LAYERS = 8
SEGMENTS = 255
LAYER_PARAMS = 607
SEGMENT_PARAMS = 615
TICK_PARAMS = 2655
LAYER_RGB = [(255, 46, 55), (57, 143, 255), (63, 220, 128), (255, 190, 55),
             (188, 112, 255), (50, 222, 225), (255, 130, 48), (245, 244, 240)]


def binding(component):
    mapping = component["handle remapping"]["map"]
    assert len(mapping) == 1
    return int(mapping[0]["value"].rsplit(" ", 1)[1])


def entries(tabs, name):
    return next(t for t in tabs if t["tabName"] == name)["componentDefinition"]["componentsData"]


def main():
    manifest = json.loads((ROOT / "plugins/TrollfxRta/plugin.json").read_text())
    assert (manifest["name"], manifest["version"], manifest["uid"], manifest["numInputs"], manifest["numOutputs"]) == (
        "TROLLfx - VST - RTA", "1.0.0", "TfRA", 2, 2)
    tabs = json.loads((SKIN / "TUI.json").read_text())["pageData"]["tabs"]
    led = entries(tabs, "SWEEP LED RESULTS")
    graph = entries(tabs, "SWEEP GRAPH")
    sweep = entries(tabs, "SWEEP")
    saved = entries(tabs, "SAVED SWEEPS")

    led_bands = [e for e in led if e["componentData"]["name"].startswith("sweepLedBand")]
    assert len(led_bands) == 31 and {binding(e) for e in led_bands} == set(range(124, 155))
    assert not any(e["componentData"]["name"].startswith(("sweepLedPeak", "sweepLedClip")) for e in led)
    assert {binding(e) for e in led if "DbTick" in e["componentData"]["name"]} == set(range(254, 259))

    lines = [e for e in graph if e["componentData"]["name"].startswith("graphLayer")]
    assert len(lines) == LAYERS * SEGMENTS
    assert {binding(e) for e in lines} == set(range(SEGMENT_PARAMS, SEGMENT_PARAMS + LAYERS * SEGMENTS))
    for layer in range(LAYERS):
        components = [e for e in lines if e["componentData"]["name"].startswith(f"graphLayer{layer}Segment")]
        assert len(components) == SEGMENTS
        assert all(e["componentData"]["type"] == "Image" for e in components)
        assert all(e["componentData"]["data"]["imageType"] == "FilmStrip" for e in components)
        assert all(e["componentData"]["data"]["numFrames"] == 962 for e in components)
        assert all(e["componentData"]["data"]["image"] == f"sweep-graph-segments-{layer}.png" for e in components)
        atlas = Image.open(SKIN / f"sweep-graph-segments-{layer}.png")
        assert atlas.mode == "RGBA" and atlas.size == (4, 64 * 962)
        assert atlas.getpixel((0, 0))[:3] == LAYER_RGB[layer]
        assert atlas.crop((0, 64 * 961, 4, 64 * 962)).getbbox() is None
        assert atlas.crop((0, 0, 4, 64)).getbbox() is not None
    assert {binding(e) for e in graph if "DbTick" in e["componentData"]["name"]} == set(range(TICK_PARAMS, TICK_PARAMS + 5))
    panel_source = (ROOT / "scripts/generate-force-rta-skin.py").read_text()
    assert "for i,(name,color) in enumerate(zip(LAYER_NAMES,LAYER_COLORS))" in panel_source

    selectors = [e for e in saved if e["componentData"]["name"].startswith("sweep-slot-")]
    assert len(selectors) == 8
    assert {binding(e) for e in selectors} == set(range(LAYER_PARAMS, LAYER_PARAMS + 8))
    for i, button in enumerate(selectors):
        data = button["componentData"]["data"]
        assert button["componentData"]["type"] == "Button"
        assert data["buttonId"] == 1 and data["numButtonsInGroup"] == 1
        assert data["onImage"] == f"sweep-slot-{i}_on.png"
        assert data["offImage"] == f"sweep-slot-{i}_off.png"
        assert ImageChops.difference(Image.open(SKIN / data["onImage"]), Image.open(SKIN / data["offImage"])).getbbox()

    for page in (led, graph):
        names = [e["componentData"]["name"] for e in page]
        assert not any("Slider" in n or "Knob" in n for n in names)
        assert any(binding(e) == 217 for e in page if e["handle remapping"]["map"])
    assert not any("smoothing" in e["componentData"]["name"].lower() for e in sweep + saved)

    qlinks = json.loads((SKIN / "Q-Links.json").read_text())["Screen Mode Q-Links"]["map"]
    mappings = {(q["Tab"], q["SubTab"]): q["Q-Links"] for q in qlinks}
    saved_q = mappings[(6, 3)]
    assert [saved_q[f"Q-Link {i}"] for i in range(1, 9)] == list(range(607, 615))
    assert all(saved_q[f"Q-Link {i}"] == -1 for i in range(9, 17))
    assert mappings[(6, 1)]["Q-Link 1"] == mappings[(6, 2)]["Q-Link 1"] == 123

    source = (ROOT / "plugins/TrollfxRta/src/TrollfxRta.cpp").read_text()
    assert "kSweepLayerEnable0=kSweepActualRawDb0+31" in source
    assert "makeSweepOverlayDisplay" in source and "kSweepLayerGraphSegments=255" in source
    assert "i.version=1000" in source
    print("PASS V1.0 skin: 8 persistent independent toggles, 8 colored connected graph layers, shared-axis ticks, red primary LED view, legacy UID/parameter indices")


if __name__ == "__main__":
    main()
