#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.6 plugin directory>" >&2; exit 2; }
folder="$(cd "$1" && pwd)"; skins="$folder/Plugin Skins"
arm="$folder/VST/trollfx-rta-arm32.so"; arm64="$folder/VST/trollfx-rta-arm64.so"
service="$folder/Service/trollfx-rta-v02-armv7"; cli="$folder/Service/trollfx-sweep-cli-armv7"
[[ -f "$folder/plugin.json" && -s "$skins/TUI.json" && -s "$skins/Q-Links.json" && -s "$skins/sweep-graph-segments.png" && -f "$arm" && -f "$arm64" && -x "$service" && -x "$cli" ]] || { echo "V0.5.6 package files missing" >&2; exit 1; }
python3 - "$folder/plugin.json" "$skins/TUI.json" "$skins/sweep-graph-segments.png" <<'PY'
import json,struct,sys
p=json.load(open(sys.argv[1])); tabs=json.load(open(sys.argv[2]))['pageData']['tabs']
assert (p['name'],p['uid'],p['version'],p['numInputs'],p['numOutputs'])==('TROLLfx - VST - RTA','TfRA','0.5.6',2,2)
def entries(name): return next(t for t in tabs if t['tabName']==name)['componentDefinition']['componentsData']
def handle(e): return int(e['handle remapping']['map'][0]['value'].rsplit(' ',1)[1])
def has_handle(es,index): return any(m.get('value')==f'Parameter {index}' for e in es for m in e.get('handle remapping',{}).get('map',[]))
sweep=entries('SWEEP')
for name,param in (('sweep-start-touch',115),('sweep-cancel-touch',116)):
    e=next(c for c in sweep if c['componentData']['name']==name); d=e['componentData']['data']
    assert e['componentData']['type']=='Button' and handle(e)==param
    assert (d['buttonId'],d['numButtonsInGroup'],d['gestureBehaviour'])==(1,1,'Instant')
    assert d['onImage'] and d['offImage']
saved=entries('SAVED SWEEPS')
buttons=[e for e in saved if e['componentData']['name'].startswith('sweep-slot-')]
assert len(buttons)==8 and {handle(e) for e in buttons}=={123}
assert {e['componentData']['data']['buttonId'] for e in buttons}==set(range(8))
assert all(e['componentData']['data']['numButtonsInGroup']==8 for e in buttons)
led=entries('SWEEP LED RESULTS'); graph=entries('SWEEP GRAPH')
assert len([e for e in led if e['componentData']['name'].startswith('sweepLedBand')])==31
segments=[e for e in graph if e['componentData']['name'].startswith('graphLineSegment')]
assert len(segments)==255 and {handle(e) for e in segments}==set(range(259,514))
assert all(e['componentData']['data']['numFrames']==962 for e in segments)
assert len([e for e in led if e['componentData']['name'].startswith('sweepLedBand')])==31
for page in (led,graph):
    assert has_handle(page,217) and has_handle(page,254) and has_handle(page,258)
    assert not any(m.get('value') in ('Parameter 122','Parameter 123','Parameter 250','Parameter 251') for e in page for m in e.get('handle remapping',{}).get('map',[]))
with open(sys.argv[3],'rb') as f:
    f.seek(16);w,h=struct.unpack('>II',f.read(8))
assert (w,h)==(4,61568),(w,h)
assert len(buttons)==8 and {e['componentData']['data']['buttonId'] for e in buttons}==set(range(8))
print('V0.5.6 manifest, controls, 8-way saved selector, 31-band auto-fit view and 255 connected graph segments: OK')
PY
nm -D --defined-only "$arm" | grep -q VSTPluginMain
readelf -h "$arm" | grep -q 'Machine:.*ARM'; readelf -A "$arm" | grep -q 'Tag_ABI_VFP_args: VFP registers'
readelf -h "$arm64" | grep -q 'Machine:.*AArch64'; nm -D --defined-only "$arm64" | grep -q VSTPluginMain
for bin in "$service" "$cli"; do readelf -h "$bin" | grep -q 'Machine:.*ARM'; readelf -h "$bin" | grep -q 'hard-float ABI'; done
for cal in 7033298.txt 7033298_90deg.txt; do [[ -s "$folder/Service/calibration/$cal" ]] && grep -F 'Sens Factor =2.7673dB' "$folder/Service/calibration/$cal" >/dev/null; done
[[ -s "$folder/Service/PFFFT-LICENSE.txt" && -s "$folder/THIRD_PARTY_NOTICES.txt" && -s "$folder/FORCE-NATIVE-V056-VALIDATION.md" ]]
if [[ -f "$folder/SHA256SUMS" ]]; then (cd "$folder" && sha256sum -c SHA256SUMS); fi
echo "V0.5.6 ARM package integrity: OK"
