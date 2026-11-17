#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then echo "Usage: $0 <plugin-folder>" >&2; exit 2; fi
folder="$(cd "$1" && pwd)"
arm="$folder/VST/trollfx-rta-arm32.so"
service="$folder/Service/trollfx-rta-v02-armv7"
[[ -f "$folder/plugin.json" && -f "$folder/Plugin Skins/TUI.json" && -f "$arm" && -x "$service" ]] || { echo "Required V0.4 plugin or service files are missing." >&2; exit 1; }
file "$arm" "$service"
readelf -h "$arm" | grep -E 'Class:|Machine:|Type:'
readelf -A "$arm" | grep -E 'Tag_CPU_arch:|Tag_ABI_VFP_args:'
nm -D --defined-only "$arm" | grep -E 'VSTPluginMain'
readelf -h "$service" | grep -E 'Class:|Machine:|Type:'
readelf -A "$service" | grep -E 'Tag_CPU_arch:|Tag_ABI_VFP_args:'
readelf -d "$service" | grep -E 'NEEDED'
python3 - "$service" <<'PY'
import re,subprocess,sys
text=subprocess.check_output(['readelf','--version-info',sys.argv[1]],text=True)
versions=[tuple(map(int,m)) for m in re.findall(r'GLIBC_(\d+)\.(\d+)',text)]
assert not versions or max(versions)<=(2,30), f'unsupported GLIBC requirement: {max(versions)}'
print('Analyzer GLIBC requirement:', '.'.join(map(str,max(versions))) if versions else 'none')
PY
for cal in 7033298.txt 7033298_90deg.txt; do [[ -s "$folder/Service/calibration/$cal" ]] || { echo "Missing calibration/$cal" >&2; exit 1; }; done
python3 - "$folder/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1]))
assert p['name']=='TROLLfx - VST - RTA' and p['uid']=='TfRA' and p['numInputs']==2 and p['numOutputs']==2 and p['version']=='0.4.0'
print('VST metadata/UID/I/O/version: OK',p['uid'],p['version'])
PY
grep -F 'Sens Factor =2.7673dB' "$folder/Service/calibration/7033298.txt" >/dev/null
grep -F 'Sens Factor =2.7673dB' "$folder/Service/calibration/7033298_90deg.txt" >/dev/null
echo "V0.4 package integrity: OK"
