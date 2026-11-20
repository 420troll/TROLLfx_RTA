#!/usr/bin/env bash
set -euo pipefail
folder="${1:-dist/force-native-v05-dsp-candidate}"
arm="$folder/Service/trollfx-sweep-cli-armv7"
[[ -x "$arm" && -f "$folder/SHA256SUMS" && -f "$folder/calibration/7033298.txt" && -f "$folder/calibration/7033298_90deg.txt" ]] || { echo "DSP package files missing" >&2; exit 1; }
readelf -h "$arm" | grep -q 'Machine:.*ARM'
readelf -h "$arm" | grep -q 'hard-float ABI'
! readelf -d "$arm" | grep -q 'libstdc++'
max_glibc="$(readelf --version-info "$arm" | grep -o 'GLIBC_[0-9.]*' | sort -V | tail -1 | sed 's/GLIBC_//')"
[[ -z "$max_glibc" || "$(printf '%s\n' 2.29 "$max_glibc" | sort -V | tail -1)" == "2.29" ]] || { echo "unsupported glibc symbol version $max_glibc" >&2; exit 1; }
(cd "$folder" && sha256sum -c SHA256SUMS)
echo "Sweep DSP ARMv7 package integrity: OK"
