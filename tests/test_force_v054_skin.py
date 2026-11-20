"""Binding and bounds checks for the V0.5.4 Force-native results pages."""
import json
import re
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
SKIN=ROOT/"plugins/TrollfxRta/skins"

def handle(e):
    maps=e["handle remapping"]["map"]
    assert len(maps)==1
    return int(maps[0]["value"].split()[-1])

def main():
    manifest=json.loads((ROOT/"plugins/TrollfxRta/plugin.json").read_text())
    assert (manifest["version"],manifest["uid"],manifest["numInputs"],manifest["numOutputs"])==("0.5.5","TfRA",2,2)
    tabs=json.loads((SKIN/"TUI.json").read_text())["pageData"]["tabs"]
    assert {t["tabName"] for t in tabs}>={"SWEEP LED RESULTS","SWEEP GRAPH"}
    led_t=next(t for t in tabs if t["tabName"]=="SWEEP LED RESULTS")
    graph_t=next(t for t in tabs if t["tabName"]=="SWEEP GRAPH")
    assert (led_t["fnKeyIndex"],led_t["fnKeySubIndex"])==(5,0)
    assert (graph_t["fnKeyIndex"],graph_t["fnKeySubIndex"])==(5,1)
    led=led_t["componentDefinition"]["componentsData"]
    graph=graph_t["componentDefinition"]["componentsData"]
    assert len([e for e in led if e["componentData"]["name"].startswith("sweepLedBand")])==31
    assert {handle(e) for e in led if e["componentData"]["name"].startswith("sweepLedBand")}==set(range(124,155))
    assert len([e for e in led if e["componentData"]["name"].startswith("sweepLedClip")])==31
    for e in led:
        name=e["componentData"]["name"]
        if name.startswith(("sweepLedBand","sweepLedPeak","sweepLedClip")):
            x,y,w,h=map(float,e["bounds"]["bounds"].split())
            assert y>=54 and y+h<=450 and x>=55 and x+w<=1010
    segments=[e for e in graph if e["componentData"]["name"].startswith("graphLineSegment")]
    assert len(segments)==255, "255 connected segments represent 256 high-resolution log points"
    assert {handle(e) for e in segments}==set(range(259,514))
    for e in segments:
        data=e["componentData"]["data"]
        assert e["componentData"]["type"]=="Image"
        assert data["imageType"]=="FilmStrip" and data["image"]=="sweep-graph-segments.png"
        assert data["numFrames"]==16642
        assert e["bounds"]["acceptsHWFocus"]=="No"
    assert not any(e["componentData"]["type"]=="Plotter" for e in graph)
    assert len([e for e in graph if e["componentData"]["name"].startswith("graphClip")])==31
    for page in (led,graph):
        names={e["componentData"]["name"] for e in page}
        for idx,prefix in ((122,"ResultControl"),(123,"ResultControl"),(250,"ResultControl"),(251,"ResultControl")):
            assert f"{'led' if page is led else 'graph'}{prefix}{idx}" in names
        for i in range(5):
            assert any(handle(e)==254+i for e in page if e["componentData"]["name"].endswith(f"DbTick{i}"))
        assert any(handle(e)==252 for e in page if e["componentData"]["name"].endswith("LowLabel"))
        assert any(handle(e)==253 for e in page if e["componentData"]["name"].endswith("HighLabel"))
        status=next(e for e in page if e["componentData"]["name"].endswith("ResultStatus"))
        assert handle(status)==217
    for img in ("sweep-graph-panel.png","sweep-led-panel.png"):
        assert Image.open(SKIN/img).size==(1280,628)
    strip=Image.open(SKIN/"sweep-graph-segments.png")
    assert strip.mode=="RGBA" and strip.size==(4,64*16642)
    # Same-scale default, output-only accurate dB readouts, appended parameter
    # blocks and old indices are explicit in the C++ plugin declaration.
    src=(ROOT/"plugins/TrollfxRta/src/TrollfxRta.cpp").read_text()
    assert re.search(r"kSweepResultStatus=kSweepRawDb0\+31",src)
    assert re.search(r"kSweepGraphPeak0=kSweepResultStatus\+1",src)
    assert re.search(r"kSweepDiagnostic=kSweepGraphPeak0\+31",src)
    assert "kSweepDisplayRange=kSweepDiagnostic+1" in src
    assert "kSweepGraphSegmentCount=255" in src
    assert "kSweepActualSmoothDb0=kSweepClipState0+31" in src
    q=json.loads((SKIN/"Q-Links.json").read_text())
    resultmaps=q["Screen Mode Q-Links"]["map"]
    for sub in (1,2):
        m=next(x for x in resultmaps if (x["Tab"],x["SubTab"])==(6,sub))["Q-Links"]
        assert [m[f"Q-Link {i}"] for i in (5,6,7,8)]==[122,123,250,251]
    print("PASS V0.5.5 skin preserves V0.5.4 full-range 31-band results and connected 256-point high-resolution graph via a parameter-bound image strip")

if __name__=="__main__": main()
