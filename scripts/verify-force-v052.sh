#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.2 plugin directory>" >&2; exit 2; }
folder="$(cd "$1" && pwd)"; arm="$folder/VST/trollfx-rta-arm32.so"; service="$folder/Service/trollfx-rta-v02-armv7"; cli="$folder/Service/trollfx-sweep-cli-armv7"; tui="$folder/Plugin Skins/TUI.json"
[[ -f "$folder/plugin.json" && -f "$tui" && -f "$arm" && -x "$service" && -x "$cli" ]] || { echo "V0.5.2 package files missing" >&2; exit 1; }
python3 - "$folder/plugin.json" "$tui" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); j=json.load(open(sys.argv[2])); tabs=j['pageData']['tabs']; assert (p['name'],p['uid'],p['version'],p['numInputs'],p['numOutputs'])==('TROLLfx - VST - RTA','TfRA','0.5.2',2,2)
t=next(x for x in tabs if x['tabName']=='SWEEP RESULTS'); es=t['componentDefinition']['componentsData']; plots=[e for e in es if e['componentData']['type']=='Plotter']; assert len(plots)==62
handles={int(e['handle remapping']['map'][0]['value'].split()[-1]):e for e in plots}; assert set(handles)==set(range(155,217))
for i in range(31):
    for idx in (155+i,186+i):
        d=handles[idx]['componentData']['data']; assert d['drawHistory'] is False and d['dataMin']==-30 and d['dataMax']==6 and d['lineDirection']=='Right'
        assert handles[155+i]['bounds']['bounds']==handles[186+i]['bounds']['bounds']
leds=[e for e in es if e['componentData']['name'].startswith('responseBand')]; assert len(leds)==31
ledhandles={int(e['handle remapping']['map'][0]['value'].split()[-1]) for e in leds}; assert ledhandles==set(range(124,155))
controls={int(e['handle remapping']['map'][0]['value'].split()[-1]) for e in es if e['componentData']['name'].startswith('resultControl') and e['handle remapping']['map']}; assert controls=={122,123}
assert any(e['componentData']['name']=='resultStatus' and e['handle remapping']['map'][0]['value']=='Parameter 217' for e in es)
print('V0.5.2 TfRA stereo manifest and dual static log-frequency Plotter + LED page: OK')
PY
nm -D --defined-only "$arm" | grep -q VSTPluginMain
readelf -h "$arm" | grep -q 'Machine:.*ARM'; readelf -A "$arm" | grep -q 'Tag_ABI_VFP_args: VFP registers'
for bin in "$service" "$cli"; do readelf -h "$bin" | grep -q 'Machine:.*ARM'; readelf -h "$bin" | grep -q 'hard-float ABI'; done
for cal in 7033298.txt 7033298_90deg.txt; do [[ -s "$folder/Service/calibration/$cal" ]] && grep -F 'Sens Factor =2.7673dB' "$folder/Service/calibration/$cal" >/dev/null; done
[[ -s "$folder/FORCE-NATIVE-V052-RESULTS.md" && -s "$folder/FORCE-NATIVE-V052-HARDWARE-TEST.md" && -s "$folder/Service/PFFFT-LICENSE.txt" ]]
if [[ -f "$folder/SHA256SUMS" ]]; then (cd "$folder" && sha256sum -c SHA256SUMS); fi
echo "V0.5.2 ARM package integrity: OK"
