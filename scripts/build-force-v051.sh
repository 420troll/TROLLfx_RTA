#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -z "${MPC_SDK_DIR:-}" || ! -f "$MPC_SDK_DIR/tools/build.py" || -z "${SDK_ZIG_PYTHON:-}" ]]; then echo "Set MPC_SDK_DIR and SDK_ZIG_PYTHON." >&2; exit 2; fi
zigcc=("$SDK_ZIG_PYTHON" -m ziglang cc); zigcxx=("$SDK_ZIG_PYTHON" -m ziglang c++)
python3 "$repo/scripts/generate-force-rta-skin.py"
python3 "$MPC_SDK_DIR/tools/build.py" "$repo/plugins/TrollfxRta" --target all --zig "$SDK_ZIG_PYTHON"
plugin="$repo/plugins/TrollfxRta/build/TROLLfx - VST - TROLLfx - VST - RTA"
python3 "$MPC_SDK_DIR/tools/validate.py" "$plugin" --strict
bash "$repo/scripts/test-force-v04.sh" "$repo/plugins/TrollfxRta/build/native/trollfx---vst---rta.so"
bash "$repo/scripts/test-sweep-dsp.sh"
for t in test_sweep_fixture.py test_sweep_comparison.py test_sweep_cli.py test_sweep_service_api.py test_sweep_smoothing_reference.py; do python3 -m unittest discover -s "$repo/tests" -p "$t" -v; done
tmp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v051-tests.XXXXXX")"; trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/VST" "$tmp/Service/calibration" "$tmp/Service/sessions"
cp "$repo/plugins/TrollfxRta/build/native/trollfx---vst---rta.so" "$tmp/VST/host.so"
cp "$repo/calibration/7033298.txt" "$repo/calibration/7033298_90deg.txt" "$tmp/Service/calibration/"
c++ -std=c++17 -O2 "$repo/tests/make_sweep_capture.cpp" "$repo/src/sweep/sweep_dsp.cpp" -o "$tmp/make-capture"
"$tmp/make-capture" "$tmp/capture.wav"
python3 "$repo/tests/test_force_v05_sweep_playback.py" "${TMPDIR:-/tmp}/trollfx-rta-v04-tests/force-vst-host-test" "$tmp/VST/host.so" "$tmp/capture.wav"
package="$repo/dist/force-native-v051-package"; mkdir -p "$package"; rm -rf "$package/$(basename "$plugin")"; mkdir -p "$plugin/Service/calibration" "$plugin/Service/sessions"
"${zigcc[@]}" -target arm-linux-gnueabihf.2.29 -mcpu=cortex_a17 -O3 -s -std=c11 -Wall -Wextra -I"$repo/src" -I"$repo/src/third_party/pffft" "$repo/src/trollfx_rta.c" "$repo/src/dsp.c" "$repo/src/third_party/pffft/pffft.c" "$repo/src/third_party/pffft/pffft_common.c" -o "$plugin/Service/trollfx-rta-v02-armv7" -lm -lpthread -ldl
"${zigcxx[@]}" -target arm-linux-gnueabihf.2.29 -mcpu=cortex_a17 -O3 -s -std=c++17 "$repo/tools/sweep_cli.cpp" "$repo/src/sweep/sweep_dsp.cpp" -o "$plugin/Service/trollfx-sweep-cli-armv7"
cp "$repo/calibration/7033298.txt" "$repo/calibration/7033298_90deg.txt" "$plugin/Service/calibration/"
cp "$repo/src/third_party/pffft/LICENSE.txt" "$plugin/Service/PFFFT-LICENSE.txt"; cp "$repo/plugins/TrollfxRta/THIRD_PARTY_NOTICES.txt" "$plugin/THIRD_PARTY_NOTICES.txt"
cp "$repo/docs/SWEEP-LAB-V051-SMOOTHING.md" "$plugin/SWEEP-LAB-V051-SMOOTHING.md"
cp "$repo/docs/SWEEP-LAB-V051-TEST-RESULTS.md" "$plugin/SWEEP-LAB-V051-TEST-RESULTS.md"
cp "$repo/docs/SWEEP-LAB-V05-HARDWARE-ACCEPTANCE.md" "$plugin/HARDWARE-ACCEPTANCE.md"
cp "$repo/scripts/install-force-v051.sh" "$repo/scripts/rollback-force-v051.sh" "$repo/scripts/verify-force-v051.sh" "$plugin/"
cp -a "$plugin" "$package/"
rm -f "$package/$(basename "$plugin")/install-force-v05.sh" "$package/$(basename "$plugin")/rollback-force-v05.sh" "$package/$(basename "$plugin")/verify-force-v05.sh" "$package/$(basename "$plugin")/README-SWEEP-LAB.md"
cp "$repo/docs/SWEEP-LAB-V051-SMOOTHING.md" "$repo/docs/SWEEP-LAB-V051-TEST-RESULTS.md" "$repo/docs/FORCE-NATIVE-V04.md" "$package/"
cp "$repo/scripts/verify-force-v051.sh" "$package/"
python3 - "$plugin/Service/README.txt" <<'PY'
import sys
open(sys.argv[1],'w').write('TROLLfx Sweep Lab V0.5.1. The existing V0.4 live RTA and V0.5 sweep service are preserved. Fractional-octave response smoothing is an offline, reversible display/export transformation. Raw capture, reference, impulse response and frequency-response.csv are retained. Use trollfx-sweep-cli-armv7 reprocess SESSION [none|1/48|1/24|1/12|1/6|1/3] to re-render an existing session. Keep Force playback on Internal Audio. Absolute SPL status remains provisional.\n')
PY
# README was written after the first package copy; sync it into the package.
cp "$plugin/Service/README.txt" "$package/$(basename "$plugin")/Service/README.txt"
bash "$repo/scripts/verify-force-v051.sh" "$package/$(basename "$plugin")"
bash "$repo/scripts/test-force-v051-package.sh" "$package/$(basename "$plugin")"
(
 cd "$package"; find "$(basename "$plugin")" -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS; sha256sum -c SHA256SUMS
)
tar -C "$repo/dist" -czf "$repo/dist/force-native-v051-package.tar.gz" force-native-v051-package
(cd "$repo/dist" && sha256sum force-native-v051-package.tar.gz > force-native-v051-package.tar.gz.sha256)
echo "V0.5.1 Force hardware-test package: $package"
