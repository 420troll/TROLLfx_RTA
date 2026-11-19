"""Static contract and artwork checks for the MPC V0.5.2 results page."""
import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SKIN = ROOT / "plugins/TrollfxRta/skins"


def main():
    manifest = json.loads((ROOT / "plugins/TrollfxRta/plugin.json").read_text())
    assert (manifest["version"], manifest["uid"], manifest["numInputs"], manifest["numOutputs"]) == ("0.5.2", "TfRA", 2, 2)
    tabs = json.loads((SKIN / "TUI.json").read_text())["pageData"]["tabs"]
    results = next(tab for tab in tabs if tab["tabName"] == "SWEEP RESULTS")
    entries = results["componentDefinition"]["componentsData"]
    plots = [entry for entry in entries if entry["componentData"]["type"] == "Plotter"]
    assert len(plots) == 62, "two static 31-point Plotter curves are required"
    plotted = {}
    for entry in plots:
        d = entry["componentData"]["data"]
        idx = int(entry["handle remapping"]["map"][0]["value"].split()[-1])
        plotted[idx] = (d, entry["bounds"]["bounds"])
        assert d["version"] == 2 and d["drawHistory"] is False
        assert d["dataMin"] == -30 and d["dataMax"] == 6 and d["lineDirection"] == "Right"
    assert set(plotted) == set(range(155, 217)), "appended raw/smoothed curve parameter indices changed"
    for i in range(31):
        assert plotted[155+i][1] == plotted[186+i][1], "raw/smoothed frequency tiles must be aligned"
    smooth_widths = [float(plotted[155+i][1].split()[2]) for i in range(31)]
    assert min(smooth_widths) > 15 and max(smooth_widths) < 60
    led_indices = sorted(int(e["handle remapping"]["map"][0]["value"].split()[-1])
                         for e in entries if e["componentData"]["type"] == "Image"
                         and e["componentData"]["name"].startswith("responseBand"))
    assert led_indices == list(range(124, 155)), "31 saved-response LED parameters changed"
    controls = {int(e["handle remapping"]["map"][0]["value"].split()[-1])
                for e in entries if e["componentData"]["name"].startswith("resultControl")
                and e["handle remapping"]["map"]}
    assert controls == {122, 123}, "result page must expose existing smoothing and saved measurement selectors"
    status = [e for e in entries if e["componentData"]["name"] == "resultStatus"]
    assert len(status) == 1 and status[0]["handle remapping"]["map"][0]["value"] == "Parameter 217"
    assert not any("svg" in json.dumps(e).lower() for e in entries), "SVG must not be claimed as an MPC skin image"

    image = Image.open(SKIN / "response-panel.png").convert("RGBA")
    assert image.size == (1280, 628)
    # Grid, lower LED region and visible log-frequency tick labels are part of
    # the static skin artwork; dynamic traces are supplied by native Plotter.
    assert image.getpixel((82, 77))[3] > 0 and image.getpixel((15, 363))[3] > 0
    for x in (82, 238, 474, 710, 946, 1260):
        assert image.getpixel((x, 120))[3] > 0
    print("PASS Force V0.5.2 skin: raw/smoothed Plotter, 31 LED bands, selectors, axes and artwork")


if __name__ == "__main__":
    main()
