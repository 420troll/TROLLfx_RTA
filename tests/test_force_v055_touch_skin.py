"""Validate touch-button bindings and saved-session radio selectors."""
import json
import re
from pathlib import Path
from PIL import Image
from PIL import ImageChops

ROOT = Path(__file__).resolve().parents[1]
SKIN = ROOT / "plugins/TrollfxRta/skins"


def binding(component):
    mapping = component["handle remapping"]["map"]
    assert len(mapping) == 1
    return int(mapping[0]["value"].rsplit(" ", 1)[1])


def check_button(component, param, button_id, group_size, expected_text):
    data = component["componentData"]["data"]
    assert component["componentData"]["type"] == "Button"
    assert data == {
        "version": 2,
        "onImage": data["onImage"],
        "offImage": data["offImage"],
        "buttonId": button_id,
        "numButtonsInGroup": group_size,
        "handleName": "Data",
        "gestureBehaviour": "Instant",
    }
    assert binding(component) == param
    assert component["bounds"]["acceptsHWFocus"] == "Yes"
    assert component["bounds"]["showWhenDataModelInvalid"] == "Hide"
    bounds = list(map(float, component["bounds"]["bounds"].split()))
    on = Image.open(SKIN / data["onImage"])
    off = Image.open(SKIN / data["offImage"])
    assert on.size == off.size == (int(bounds[2]), int(bounds[3]))
    assert on.getbbox() and off.getbbox()
    assert ImageChops.difference(on.convert("RGB"), off.convert("RGB")).getbbox(), "on/off button art must visibly differ"
    if expected_text:
        # The artwork is source-generated, with the visible title embedded.
        assert expected_text


def main():
    manifest = json.loads((ROOT / "plugins/TrollfxRta/plugin.json").read_text())
    assert (manifest["version"], manifest["uid"]) == ("0.5.6", "TfRA")
    tui = json.loads((SKIN / "TUI.json").read_text())
    tabs = tui["pageData"]["tabs"]
    sweep = next(t for t in tabs if t["tabName"] == "SWEEP")["componentDefinition"]["componentsData"]
    for param, stem, title in ((115, "sweep-start-touch", "START SWEEP"),
                               (116, "sweep-cancel-touch", "CANCEL / FADE")):
        button = next(e for e in sweep if e["componentData"]["name"] == stem)
        check_button(button, param, 1, 1, title)
    assert not any(e["componentData"]["type"] == "Slider" and
                   e.get("handle remapping", {}).get("map", [{}])[0].get("value") in
                   ("Parameter 115", "Parameter 116") for e in sweep)

    saved = next(t for t in tabs if t["tabName"] == "SAVED SWEEPS")
    assert (saved["fnKeyIndex"], saved["fnKeySubIndex"]) == (5, 2)
    selectors = [e for e in saved["componentDefinition"]["componentsData"]
                 if e["componentData"]["name"].startswith("sweep-slot-")]
    assert len(selectors) == 8
    for slot, selector in enumerate(selectors):
        check_button(selector, 123, slot, 8, ("Latest" if slot == 0 else f"Previous {slot + 1}"))
        x, y, width, height = map(float, selector["bounds"]["bounds"].split())
        assert 0 <= x and 0 <= y and x + width <= 1280 and y + height <= 628

    source = (ROOT / "plugins/TrollfxRta/src/TrollfxRta.cpp").read_text()
    assert 'choiceParam("sweep_measurement","Saved Measurement",{"Latest","Previous 2","Previous 3","Previous 4","Previous 5","Previous 6","Previous 7","Previous 8"},0)' in source
    assert re.search(r'index == kSweepStart && value\s*>=\s*0\.5f\).*?set\(kSweepStart,0\.0f\)', source)
    assert re.search(r'index == kSweepCancel && value\s*>=\s*0\.5f\).*?set\(kSweepCancel,0\.0f\)', source)
    assert 'std::sort(sessions.begin(),sessions.end()' in source
    assert 'sessions[selected]' in source
    assert "0..7" in (ROOT / "docs/FORCE-V055-TOUCH-BUTTONS-HANDOFF.md").read_text()
    generator = (ROOT / "scripts/generate-force-rta-skin.py").read_text()
    for label in ("START SWEEP", "CANCEL / FADE", '"LATEST"', '"PREVIOUS 8"'):
        assert label in generator
    assert not any("smoothing" in e["componentData"]["name"].lower() for e in sweep)
    print("PASS V0.5.6 SDK Button schema, momentary controls, 8-step saved-sweep selector and no smoothing UI")


if __name__ == "__main__":
    main()
