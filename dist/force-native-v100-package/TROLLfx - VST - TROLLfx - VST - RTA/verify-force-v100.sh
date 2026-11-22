#!/bin/sh
set -eu
[ "$#" -eq 1 ] || { echo "Usage: sh $0 <V1.0.0 plugin folder>" >&2; exit 2; }
folder=$(cd "$1" && pwd)
[ -f "$folder/plugin.json" ] && [ -s "$folder/Plugin Skins/TUI.json" ] && \
 [ -s "$folder/Plugin Skins/Q-Links.json" ] && [ -s "$folder/Service/trollfx-rta-v02-armv7" ] && \
 [ -s "$folder/VST/trollfx-rta-arm32.so" ] && [ -s "$folder/VST/trollfx-rta-arm64.so" ] || {
    echo 'V1.0.0 required files are missing' >&2; exit 1;
}
grep -F '"uid": "TfRA"' "$folder/plugin.json" >/dev/null
grep -F '"version": "1.0.0"' "$folder/plugin.json" >/dev/null
grep -F '"numInputs": 2' "$folder/plugin.json" >/dev/null
grep -F '"numOutputs": 2' "$folder/plugin.json" >/dev/null
grep -F 'Parameter 614' "$folder/Plugin Skins/TUI.json" >/dev/null
grep -F 'Parameter 2654' "$folder/Plugin Skins/TUI.json" >/dev/null
[ -s "$folder/Plugin Skins/sweep-graph-segments-0.png" ] && [ -s "$folder/Plugin Skins/sweep-graph-segments-7.png" ]
[ -s "$folder/Service/calibration/7033298.txt" ] && [ -s "$folder/Service/calibration/7033298_90deg.txt" ]
[ -s "$folder/Service/PFFFT-LICENSE.txt" ] && [ -s "$folder/THIRD_PARTY_NOTICES.txt" ]
(cd "$folder" && sha256sum -c SHA256SUMS)
echo 'V1.0.0 plugin metadata, ARM artifacts, eight overlay layers, calibration and file checksums: OK'
