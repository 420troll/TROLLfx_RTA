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
python3 "$repo/tests/test_force_v054_skin.py"
g++ -O3 -std=c++17 -Wall -Wextra -Wpedantic "$repo/tools/benchmark_sweep_display.cpp" "$repo/src/sweep/sweep_display.cpp" "$repo/src/sweep/sweep_dsp.cpp" -o "$repo/build/benchmark-sweep-display-v054"
"$repo/build/benchmark-sweep-display-v054"
for t in test_sweep_fixture.py test_sweep_comparison.py test_sweep_cli.py test_sweep_service_api.py test_sweep_smoothing_reference.py; do python3 -m unittest discover -s "$repo/tests" -p "$t" -v; done
if python3 -c 'import pyfar' >/dev/null 2>&1; then python3 -m unittest discover -s "$repo/tests" -p test_sweep_pyfar.py -v; else echo "pyfar reference skipped (install requirements-sweep-reference.txt to enable)"; fi
TMPTEST="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v054-tests.XXXXXX")"; trap 'rm -rf "$TMPTEST"' EXIT
mkdir -p "$TMPTEST/VST" "$TMPTEST/Service/calibration" "$TMPTEST/Service/sessions"
cp "$repo/calibration/7033298.txt" "$repo/calibration/7033298_90deg.txt" "$TMPTEST/Service/calibration/"
c++ -std=c++17 -O2 "$repo/tests/make_sweep_capture.cpp" "$repo/src/sweep/sweep_dsp.cpp" -o "$TMPTEST/make-capture"
"$TMPTEST/make-capture" "$TMPTEST/capture.wav"
cp "$repo/plugins/TrollfxRta/build/native/trollfx---vst---rta.so" "$TMPTEST/VST/host.so"
python3 "$repo/tests/test_force_v05_sweep_playback.py" "${TMPDIR:-/tmp}/trollfx-rta-v04-tests/force-vst-host-test" "$TMPTEST/VST/host.so" "$TMPTEST/capture.wav"
# Rebuild deterministic ARMv7 service and sweep CLI; do not alter Force audio routing.
mkdir -p "$plugin/Service/calibration" "$plugin/Service/sessions"
rm -f "$plugin/SWEEP-LAB-V051-SMOOTHING.md" "$plugin/SWEEP-LAB-V051-TEST-RESULTS.md" \
  "$plugin/FORCE-NATIVE-V052-RESULTS.md" "$plugin/FORCE-NATIVE-V052-HARDWARE-TEST.md" \
  "$plugin/FORCE-NATIVE-V053-RESULTS.md" "$plugin/FORCE-NATIVE-V053-HARDWARE-TEST.md" \
  "$plugin/install-force-v05.sh" "$plugin/rollback-force-v05.sh" "$plugin/verify-force-v05.sh" \
  "$plugin/install-force-v051.sh" "$plugin/rollback-force-v051.sh" "$plugin/verify-force-v051.sh" \
  "$plugin/install-force-v052.sh" "$plugin/rollback-force-v052.sh" "$plugin/verify-force-v052.sh" \
  "$plugin/install-force-v053.sh" "$plugin/rollback-force-v053.sh" "$plugin/verify-force-v053.sh" \
  "$plugin/README-SWEEP-LAB.md" "$plugin/README-HARDWARE-TEST.md" "$plugin/response-panel.png" \
  "$plugin/Plugin Skins/response-panel.png"
"${zigcc[@]}" -target arm-linux-gnueabihf.2.29 -mcpu=cortex_a17 -O3 -s -std=c11 -Wall -Wextra -I"$repo/src" -I"$repo/src/third_party/pffft" "$repo/src/trollfx_rta.c" "$repo/src/dsp.c" "$repo/src/third_party/pffft/pffft.c" "$repo/src/third_party/pffft/pffft_common.c" -o "$plugin/Service/trollfx-rta-v02-armv7" -lm -lpthread -ldl
"${zigcxx[@]}" -target arm-linux-gnueabihf.2.29 -mcpu=cortex_a17 -O3 -s -std=c++17 "$repo/tools/sweep_cli.cpp" "$repo/src/sweep/sweep_dsp.cpp" -o "$plugin/Service/trollfx-sweep-cli-armv7"
cp "$repo/calibration/7033298.txt" "$repo/calibration/7033298_90deg.txt" "$plugin/Service/calibration/"
cp "$repo/src/third_party/pffft/LICENSE.txt" "$plugin/Service/PFFFT-LICENSE.txt"; cp "$repo/plugins/TrollfxRta/THIRD_PARTY_NOTICES.txt" "$plugin/THIRD_PARTY_NOTICES.txt"
cp "$repo/docs/FORCE-NATIVE-V054-RESULTS.md" "$plugin/FORCE-NATIVE-V054-RESULTS.md"
cp "$repo/docs/FORCE-NATIVE-V054-HARDWARE-TEST.md" "$plugin/FORCE-NATIVE-V054-HARDWARE-TEST.md"
cp "$repo/docs/SWEEP-LAB-V05-HARDWARE-ACCEPTANCE.md" "$plugin/HARDWARE-ACCEPTANCE.md"
cp "$repo/scripts/install-force-v054.sh" "$repo/scripts/rollback-force-v054.sh" "$repo/scripts/verify-force-v054.sh" "$plugin/"
python3 - "$plugin/Service/README.txt" <<'PY'
import sys
open(sys.argv[1],'w').write('TROLLfx Sweep Lab V0.5.4. Existing live RTA and saved sweep capture/analysis remain. Sweep results use one adjustable common relative-dB window around the smoothed 1 kHz response, 31-band LED clipping indicators, and a 256-sample log-frequency curve encoded by supported Image filmstrip frames. Range/center are reversible view controls. Six fractional-octave choices reprocess only the selected saved measurement. Raw capture/reference/impulse/response/phase data are preserved. Absolute SPL remains provisional. Keep Force playback on Internal Audio. See FORCE-NATIVE-V054-RESULTS.md and FORCE-NATIVE-V054-HARDWARE-TEST.md.\n')
PY
package="$repo/dist/force-native-v054-package"; rm -rf "$package"; mkdir -p "$package"
cp -a "$plugin" "$package/"
cp "$repo/docs/FORCE-NATIVE-V054-RESULTS.md" "$repo/docs/FORCE-NATIVE-V054-HARDWARE-TEST.md" "$repo/docs/FORCE-NATIVE-V04.md" "$package/"
cp "$repo/scripts/install-force-v054.sh" "$repo/scripts/rollback-force-v054.sh" "$repo/scripts/verify-force-v054.sh" "$repo/scripts/test-force-v054-package.sh" "$package/"
cp "$repo/docs/images/v054-sweep-led-layout-simulation.png" "$repo/docs/images/v054-sweep-graph-layout-simulation.png" "$package/"
(
 cd "$package/$(basename "$plugin")"
 find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
bash "$repo/scripts/verify-force-v054.sh" "$package/$(basename "$plugin")"
bash "$repo/scripts/test-force-v054-package.sh" "$package/$(basename "$plugin")"
(
 cd "$package"; find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS; sha256sum -c SHA256SUMS
)
tar -C "$repo/dist" -czf "$repo/dist/force-native-v054-package.tar.gz" force-native-v054-package
(cd "$repo/dist" && sha256sum force-native-v054-package.tar.gz > force-native-v054-package.tar.gz.sha256)
printf '\nV0.5.4 evaluation package: %s\n' "$package"
printf 'Package SHA256: '; sha256sum "$repo/dist/force-native-v054-package.tar.gz" | cut -d' ' -f1
printf 'ARM32 VST SHA256: '; sha256sum "$package/$(basename "$plugin")/VST/trollfx-rta-arm32.so" | cut -d' ' -f1
readelf -h "$package/$(basename "$plugin")/VST/trollfx-rta-arm32.so" | grep -E 'Class:|Machine:'
readelf -A "$package/$(basename "$plugin")/VST/trollfx-rta-arm32.so" | grep 'Tag_ABI_VFP_args'
echo "No hardware install or MPC restart performed."
