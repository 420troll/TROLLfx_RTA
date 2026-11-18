#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then echo "Usage: $0 <plugin-folder>" >&2; exit 2; fi
folder="$(cd "$1" && pwd)"
arm="$folder/VST/trollfx-rta-arm32.so"
service="$folder/Service/trollfx-rta-v02-armv7"
cli="$folder/Service/trollfx-sweep-cli-armv7"
[[ -f "$folder/plugin.json" && -f "$folder/Plugin Skins/TUI.json" && -f "$arm" && -x "$service" && -x "$cli" ]] || { echo "V0.5 package files missing" >&2; exit 1; }
python3 - "$folder/plugin.json" "$folder/Plugin Skins/TUI.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); tui=json.load(open(sys.argv[2]))
assert p['name']=='TROLLfx - VST - RTA' and p['uid']=='TfRA' and p['numInputs']==2 and p['numOutputs']==2 and p['version']=='0.5.0'
tabs=tui['pageData']['tabs']
assert any(t.get('tabName')=='SWEEP' for t in tabs), [t.get('tabName') for t in tabs]
print('VST metadata/UID/I/O/version and SWEEP page: OK',p['uid'],p['version'])
PY
nm -D --defined-only "$arm" | grep -q VSTPluginMain
readelf -h "$arm" | grep -q 'Machine:.*ARM'
readelf -A "$arm" | grep -q 'Tag_ABI_VFP_args: VFP registers'
readelf -h "$service" | grep -q 'Machine:.*ARM'
readelf -h "$service" | grep -q 'hard-float ABI'
readelf -h "$cli" | grep -q 'Machine:.*ARM'
readelf -h "$cli" | grep -q 'hard-float ABI'
! readelf -d "$cli" | grep -q 'libstdc++'
for cal in 7033298.txt 7033298_90deg.txt; do [[ -s "$folder/Service/calibration/$cal" ]] || { echo "Missing calibration $cal" >&2; exit 1; }; grep -F 'Sens Factor =2.7673dB' "$folder/Service/calibration/$cal" >/dev/null; done
if [[ -f "$folder/SHA256SUMS" ]]; then (cd "$folder" && sha256sum -c SHA256SUMS); fi
echo "V0.5 native plugin package integrity: OK"
