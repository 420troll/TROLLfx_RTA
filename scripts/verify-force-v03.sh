#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then echo "Usage: $0 <plugin-folder>" >&2; exit 2; fi
folder="$(cd "$1" && pwd)"
arm="$folder/VST/trollfx-rta-arm32.so"
[[ -f "$arm" && -f "$folder/plugin.json" && -f "$folder/Plugin Skins/TUI.json" ]] || { echo "Required plugin files are missing." >&2; exit 1; }
file "$arm"
readelf -h "$arm" | grep -E 'Class:|Machine:|Type:'
readelf -A "$arm" | grep -E 'Tag_CPU_arch:|Tag_ABI_VFP_args:'
nm -D --defined-only "$arm" | grep -E 'VSTPluginMain'
python3 - "$folder/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1]))
assert p['name']=='TROLLfx - VST - RTA' and p['uid']=='TfRA' and p['numInputs']==2 and p['numOutputs']==2
print('Plugin metadata/UID/I/O: OK', p['uid'])
PY
