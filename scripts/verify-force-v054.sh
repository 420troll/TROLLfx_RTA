#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.4 plugin directory>" >&2; exit 2; }
folder="$(cd "$1" && pwd)"; arm="$folder/VST/trollfx-rta-arm32.so"; service="$folder/Service/trollfx-rta-v02-armv7"; cli="$folder/Service/trollfx-sweep-cli-armv7"; skins="$folder/Plugin Skins"
[[ -f "$folder/plugin.json" && -f "$skins/TUI.json" && -f "$skins/Q-Links.json" && -f "$skins/sweep-graph-segments.png" && -f "$arm" && -x "$service" && -x "$cli" ]] || { echo "V0.5.4 package files missing" >&2; exit 1; }
python3 - "$folder/plugin.json" "$skins/TUI.json" "$skins/sweep-graph-segments.png" <<'PY'
import json,struct,sys
p=json.load(open(sys.argv[1])); tui=json.load(open(sys.argv[2])); tabs=tui['pageData']['tabs']
assert (p['name'],p['uid'],p['version'],p['numInputs'],p['numOutputs'])==('TROLLfx - VST - RTA','TfRA','0.5.4',2,2)
def handle(e): return int(e['handle remapping']['map'][0]['value'].split()[-1])
def has_handle(entries,index):
    return any(m.get('value')==f'Parameter {index}' for e in entries for m in e.get('handle remapping',{}).get('map',[]))
led=next(t for t in tabs if t['tabName']=='SWEEP LED RESULTS')['componentDefinition']['componentsData']
graph=next(t for t in tabs if t['tabName']=='SWEEP GRAPH')['componentDefinition']['componentsData']
assert len([e for e in led if e['componentData']['name'].startswith('sweepLedBand')])==31
segments=[e for e in graph if e['componentData']['name'].startswith('graphLineSegment')]
assert len(segments)==255 and {handle(e) for e in segments}==set(range(259,514))
assert all(e['componentData']['type']=='Image' and e['componentData']['data']['imageType']=='FilmStrip' and e['componentData']['data']['numFrames']==16642 for e in segments)
for name in ('SWEEP LED RESULTS','SWEEP GRAPH'):
    c=next(t for t in tabs if t['tabName']==name)['componentDefinition']['componentsData']
    assert all(has_handle(c,i) for i in (122,123,250,251,252,253,254,255,256,257,258,217,249))
assert not any(e['componentData']['type']=='Plotter' for e in graph)
with open(sys.argv[3],'rb') as f:
    f.seek(16);w,h=struct.unpack('>II',f.read(8))
assert (w,h)==(4,1065088), (w,h)
print('V0.5.4 manifest, full-screen result pages, 255 connected curve segments and 31-band view: OK')
PY
nm -D --defined-only "$arm" | grep -q VSTPluginMain
readelf -h "$arm" | grep -q 'Machine:.*ARM'; readelf -A "$arm" | grep -q 'Tag_ABI_VFP_args: VFP registers'
for bin in "$service" "$cli"; do readelf -h "$bin" | grep -q 'Machine:.*ARM'; readelf -h "$bin" | grep -q 'hard-float ABI'; done
for cal in 7033298.txt 7033298_90deg.txt; do [[ -s "$folder/Service/calibration/$cal" ]] && grep -F 'Sens Factor =2.7673dB' "$folder/Service/calibration/$cal" >/dev/null; done
[[ -s "$folder/Service/PFFFT-LICENSE.txt" && -s "$folder/THIRD_PARTY_NOTICES.txt" && -s "$folder/FORCE-NATIVE-V054-RESULTS.md" && -s "$folder/FORCE-NATIVE-V054-HARDWARE-TEST.md" ]]
if [[ -f "$folder/SHA256SUMS" ]]; then (cd "$folder" && sha256sum -c SHA256SUMS); fi
echo "V0.5.4 ARM package integrity: OK"
