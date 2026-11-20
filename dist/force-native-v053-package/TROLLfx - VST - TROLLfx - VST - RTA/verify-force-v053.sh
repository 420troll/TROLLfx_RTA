#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.3 plugin directory>" >&2; exit 2; }
folder="$(cd "$1" && pwd)"; arm="$folder/VST/trollfx-rta-arm32.so"; service="$folder/Service/trollfx-rta-v02-armv7"; cli="$folder/Service/trollfx-sweep-cli-armv7"; tui="$folder/Plugin Skins/TUI.json"
[[ -f "$folder/plugin.json" && -f "$tui" && -f "$arm" && -x "$service" && -x "$cli" ]] || { echo "V0.5.3 package files missing" >&2; exit 1; }
python3 - "$folder/plugin.json" "$tui" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); j=json.load(open(sys.argv[2])); tabs=j['pageData']['tabs']; names={t['tabName'] for t in tabs}
assert (p['name'],p['uid'],p['version'],p['numInputs'],p['numOutputs'])==('TROLLfx - VST - RTA','TfRA','0.5.3',2,2)
assert {'SWEEP LED RESULTS','SWEEP GRAPH'} <= names
led=next(t for t in tabs if t['tabName']=='SWEEP LED RESULTS')['componentDefinition']['componentsData']
graph=next(t for t in tabs if t['tabName']=='SWEEP GRAPH')['componentDefinition']['componentsData']
assert not any(e['componentData']['type']=='Plotter' for e in graph)
fills=[e for e in graph if e['componentData']['name'].startswith('graphFill')]; points=[e for e in graph if e['componentData']['name'].startswith('graphTracePoint')]
assert len(fills)==len(points)==31
assert {int(e['handle remapping']['map'][0]['value'].split()[-1]) for e in fills}==set(range(155,186))
assert {int(e['handle remapping']['map'][0]['value'].split()[-1]) for e in points}==set(range(218,249))
assert len([e for e in led if e['componentData']['name'].startswith('sweepLedBand')])==31
print('V0.5.3 manifest, full-screen pages, saved-curve bindings: OK')
PY
nm -D --defined-only "$arm" | grep -q VSTPluginMain
readelf -h "$arm" | grep -q 'Machine:.*ARM'; readelf -A "$arm" | grep -q 'Tag_ABI_VFP_args: VFP registers'
for bin in "$service" "$cli"; do readelf -h "$bin" | grep -q 'Machine:.*ARM'; readelf -h "$bin" | grep -q 'hard-float ABI'; done
for cal in 7033298.txt 7033298_90deg.txt; do [[ -s "$folder/Service/calibration/$cal" ]] && grep -F 'Sens Factor =2.7673dB' "$folder/Service/calibration/$cal" >/dev/null; done
[[ -s "$folder/FORCE-NATIVE-V053-RESULTS.md" && -s "$folder/FORCE-NATIVE-V053-HARDWARE-TEST.md" && -s "$folder/Service/PFFFT-LICENSE.txt" ]]
if [[ -f "$folder/SHA256SUMS" ]]; then (cd "$folder" && sha256sum -c SHA256SUMS); fi
echo "V0.5.3 ARM package integrity: OK"
