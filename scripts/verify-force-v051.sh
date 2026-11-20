#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.1 plugin folder>" >&2; exit 2; }
folder="$(cd "$1" && pwd)"; arm="$folder/VST/trollfx-rta-arm32.so"; service="$folder/Service/trollfx-rta-v02-armv7"; cli="$folder/Service/trollfx-sweep-cli-armv7"
[[ -f "$folder/plugin.json" && -f "$folder/Plugin Skins/TUI.json" && -f "$arm" && -x "$service" && -x "$cli" ]] || { echo "V0.5.1 package files missing" >&2; exit 1; }
python3 - "$folder/plugin.json" "$folder/Plugin Skins/TUI.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); tabs=json.load(open(sys.argv[2]))['pageData']['tabs']
assert (p['name'],p['uid'],p['version'],p['numInputs'],p['numOutputs'])==('TROLLfx - VST - RTA','TfRA','0.5.1',2,2)
assert any(t['tabName']=='SWEEP RESULTS' for t in tabs)
print('V0.5.1 metadata, UID TfRA, stereo I/O and SWEEP RESULTS page: OK')
PY
nm -D --defined-only "$arm" | grep -q VSTPluginMain
readelf -h "$arm" | grep -q 'Machine:.*ARM'; readelf -A "$arm" | grep -q 'Tag_ABI_VFP_args: VFP registers'
for bin in "$service" "$cli"; do readelf -h "$bin" | grep -q 'Machine:.*ARM'; readelf -h "$bin" | grep -q 'hard-float ABI'; done
for cal in 7033298.txt 7033298_90deg.txt; do [[ -s "$folder/Service/calibration/$cal" ]] && grep -F 'Sens Factor =2.7673dB' "$folder/Service/calibration/$cal" >/dev/null; done
[[ -s "$folder/SWEEP-LAB-V051-SMOOTHING.md" && -s "$folder/Service/PFFFT-LICENSE.txt" ]]
if [[ -f "$folder/SHA256SUMS" ]]; then (cd "$folder" && sha256sum -c SHA256SUMS); fi
echo "V0.5.1 ARM package integrity: OK"
