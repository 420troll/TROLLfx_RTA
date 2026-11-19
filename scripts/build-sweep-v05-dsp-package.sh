#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -z "${SDK_ZIG_PYTHON:-}" ]]; then echo "Set SDK_ZIG_PYTHON for the pinned Zig toolchain." >&2; exit 2; fi
zig=("$SDK_ZIG_PYTHON" -m ziglang)
mkdir -p "$repo_dir/build" "$repo_dir/dist/force-native-v05-dsp-candidate/Service"
"${zig[@]}" c++ -target arm-linux-gnueabihf.2.29 -mcpu=cortex_a17 -O3 -s -std=c++17 \
  "$repo_dir/tools/sweep_cli.cpp" "$repo_dir/src/sweep/sweep_dsp.cpp" \
  -o "$repo_dir/dist/force-native-v05-dsp-candidate/Service/trollfx-sweep-cli-armv7"
arm="$repo_dir/dist/force-native-v05-dsp-candidate/Service/trollfx-sweep-cli-armv7"
readelf -h "$arm" | grep -q 'Machine:.*ARM'
readelf -h "$arm" | grep -q 'hard-float ABI'
if readelf -d "$arm" | grep -q 'libstdc++'; then echo "Unexpected libstdc++ dependency in ARM sweep CLI" >&2; exit 1; fi
max_glibc="$(readelf --version-info "$arm" | grep -o 'GLIBC_[0-9.]*' | sort -V | tail -1 | sed 's/GLIBC_//')"
[[ -z "$max_glibc" || "$(printf '%s\n' 2.29 "$max_glibc" | sort -V | tail -1)" == "2.29" ]] || { echo "Sweep CLI requires glibc $max_glibc; Force baseline is 2.29." >&2; exit 1; }
file "$arm"
readelf -d "$arm" | grep NEEDED || true
cp "$repo_dir/requirements-sweep-reference.txt" "$repo_dir/dist/force-native-v05-dsp-candidate/"
cp "$repo_dir/docs/SWEEP-LAB-V05-DSP.md" "$repo_dir/dist/force-native-v05-dsp-candidate/README-DSP.md"
cp "$repo_dir/scripts/verify-sweep-v05-dsp-package.sh" "$repo_dir/dist/force-native-v05-dsp-candidate/"
mkdir -p "$repo_dir/dist/force-native-v05-dsp-candidate/calibration"
cp "$repo_dir/calibration/7033298.txt" "$repo_dir/calibration/7033298_90deg.txt" "$repo_dir/dist/force-native-v05-dsp-candidate/calibration/"
cat > "$repo_dir/dist/force-native-v05-dsp-candidate/Service/README.txt" <<'TXT'
TROLLfx Sweep Lab V0.5 offline DSP test candidate.
This ARMv7 hard-float utility analyzes completed mono WAV recordings only.
It does not start a sweep, open ALSA, control Force output routing, or replace
the V0.4 RTA plugin. Use: trollfx-sweep-cli-armv7 analyze reference.wav
capture.wav output-session [clock_ppm [start_hz stop_hz [calibration.txt [0|90]]]].
Use the included calibration/7033298.txt or _90deg.txt for frequency-response
correction. Sens Factor 2.7673 dB is retained as metadata, not used as an
absolute SPL offset; results remain PROVISIONAL for absolute SPL. This
candidate is not an installable VST update.
TXT
(
  cd "$repo_dir/dist/force-native-v05-dsp-candidate"
  sha256sum Service/trollfx-sweep-cli-armv7 calibration/7033298.txt calibration/7033298_90deg.txt > SHA256SUMS
)
bash "$repo_dir/scripts/verify-sweep-v05-dsp-package.sh" "$repo_dir/dist/force-native-v05-dsp-candidate"
tar -C "$repo_dir/dist" -czf "$repo_dir/dist/force-native-v05-dsp-candidate.tar.gz" force-native-v05-dsp-candidate
(cd "$repo_dir/dist" && sha256sum force-native-v05-dsp-candidate.tar.gz > force-native-v05-dsp-candidate.tar.gz.sha256)
echo "ARMv7 DSP candidate: $repo_dir/dist/force-native-v05-dsp-candidate"
