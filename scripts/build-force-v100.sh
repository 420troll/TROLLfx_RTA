#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
sdk="${MPC_SDK_DIR:-/tmp/mpc-plugin-builder-sdk-v056}"
zigpython="${SDK_ZIG_PYTHON:-/usr/bin/python3}"
export MPC_SDK_DIR="$sdk"
export PYTHONPATH="${TROLLFX_ZIG_PYTHONPATH:-/tmp/trollfx-zig-python}${PYTHONPATH:+:$PYTHONPATH}"
base="$repo/dist/force-native-v056-package/TROLLfx - VST - TROLLfx - VST - RTA"
[[ -f "$sdk/tools/build.py" && -x "$zigpython" ]] || { echo 'Pinned MPC SDK or Zig Python missing' >&2; exit 2; }
[[ -d "$base/Service" ]] || { echo "V0.5.6 service package missing: $base" >&2; exit 2; }
python3 -m py_compile "$repo/scripts/generate-force-rta-skin.py" "$repo/tests/test_force_v056_skin.py" "$repo/tests/test_force_v055_touch_skin.py"
python3 "$repo/scripts/generate-force-rta-skin.py"
rm -rf "$repo/plugins/TrollfxRta/build"
python3 "$sdk/tools/build.py" "$repo/plugins/TrollfxRta" --target all --zig "$zigpython"
plugin="$repo/plugins/TrollfxRta/build/TROLLfx - VST - TROLLfx - VST - RTA"
python3 "$sdk/tools/validate.py" "$plugin" --strict
native="$repo/plugins/TrollfxRta/build/native/trollfx---vst---rta.so"
bash "$repo/scripts/test-force-v04.sh" "$native"
bash "$repo/scripts/test-sweep-dsp.sh"
python3 "$repo/tests/test_force_v056_skin.py"
python3 "$repo/tests/test_force_v055_touch_skin.py"
for test in test_sweep_fixture.py test_sweep_comparison.py test_sweep_cli.py test_sweep_service_api.py test_sweep_smoothing_reference.py; do
    python3 -m unittest discover -s "$repo/tests" -p "$test" -v
done
pyfar_path="${PYFAR_PYTHONPATH:-/tmp/trollfx-v054-pyfar-packages}"
if PYTHONPATH="$pyfar_path${PYTHONPATH:+:$PYTHONPATH}" python3 -c 'import pyfar' >/dev/null 2>&1; then
    PYTHONPATH="$pyfar_path${PYTHONPATH:+:$PYTHONPATH}" python3 -m unittest discover -s "$repo/tests" -p test_sweep_pyfar.py -v
else
    echo 'pyfar reference test skipped: install requirements-sweep-reference.txt to enable it'
fi
host_tmp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v100-host.XXXXXX")"
trap 'rm -rf "$host_tmp"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$sdk/framework" "$repo/tests/test_force_vst_host.cpp" -ldl -pthread -o "$host_tmp/host-test"
c++ -std=c++17 -O2 "$repo/tests/make_sweep_capture.cpp" "$repo/src/sweep/sweep_dsp.cpp" -o "$host_tmp/make-capture"
"$host_tmp/make-capture" "$host_tmp/capture.wav"
for mode in playback cancel; do
    test_root="$host_tmp/$mode"; mkdir -p "$test_root/VST" "$test_root/Service/calibration" "$test_root/Service/sessions"
    cp "$native" "$test_root/VST/host.so"
    cp "$repo/calibration/7033298.txt" "$repo/calibration/7033298_90deg.txt" "$test_root/Service/calibration/"
    if [[ "$mode" == cancel ]]; then
        python3 "$repo/tests/test_force_v05_sweep_playback.py" "$host_tmp/host-test" "$test_root/VST/host.so" "$host_tmp/capture.wav" --sweep-cancel
    else
        python3 "$repo/tests/test_force_v05_sweep_playback.py" "$host_tmp/host-test" "$test_root/VST/host.so" "$host_tmp/capture.wav"
    fi
done
package="$repo/dist/force-native-v100-package"; name="$(basename "$plugin")"; out="$package/$name"
rm -rf "$package"; mkdir -p "$out"
cp -a "$plugin/." "$out/"
cp -a "$base/Service/." "$out/Service/"
rm -rf "$out/Service/sessions"; mkdir -p "$out/Service/sessions"
cp "$repo/docs/FORCE-NATIVE-V100-MANUAL.md" "$repo/docs/FORCE-NATIVE-V100-RENDERING.md" "$repo/docs/FORCE-NATIVE-V04.md" "$out/"
cp "$repo/docs/FORCE-NATIVE-V100-MANUAL.md" "$repo/docs/FORCE-NATIVE-V100-RENDERING.md" "$package/"
cp "$repo/plugins/TrollfxRta/LICENSE.txt" "$out/LICENSE.txt"
cp "$repo/plugins/TrollfxRta/THIRD_PARTY_NOTICES.txt" "$out/THIRD_PARTY_NOTICES.txt"
cat > "$out/Service/README.txt" <<'TXT'
TROLLfx RTA V1.0.0. Existing UMIK-1 analyzer and offline sweep engine are retained. Sweep result graph uses fixed 1/6-octave smoothing, one common dB range across enabled curves, and eight colored 256-point lines. The red 31-band LED page shows the primary selected measurement. Keep Force playback on Internal Audio. See FORCE-NATIVE-V100-MANUAL.md and FORCE-NATIVE-V100-RENDERING.md. Physical Force UI/performance acceptance is still required.
TXT
cp "$repo/scripts/verify-force-v100.sh" "$repo/scripts/install-force-v100.sh" "$repo/scripts/rollback-force-v100.sh" "$out/"
cp "$repo/scripts/verify-force-v100.sh" "$repo/scripts/install-force-v100.sh" "$repo/scripts/rollback-force-v100.sh" "$repo/scripts/test-force-v100-package.sh" "$package/"
(
 cd "$out"; find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
sh "$repo/scripts/verify-force-v100.sh" "$out"
bash "$repo/scripts/test-force-v100-package.sh" "$out"
(
 cd "$package"; find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS; sha256sum -c SHA256SUMS
)
tar -C "$repo/dist" -czf "$repo/dist/force-native-v100-package.tar.gz" force-native-v100-package
(cd "$repo/dist" && sha256sum force-native-v100-package.tar.gz > force-native-v100-package.tar.gz.sha256)
printf '\nV1.0.0 package: %s\n' "$package"
printf 'Package SHA256: '; sha256sum "$repo/dist/force-native-v100-package.tar.gz" | cut -d' ' -f1
printf 'ARM32 VST SHA256: '; sha256sum "$out/VST/trollfx-rta-arm32.so" | cut -d' ' -f1
printf 'ARM64 VST SHA256: '; sha256sum "$out/VST/trollfx-rta-arm64.so" | cut -d' ' -f1
readelf -h "$out/VST/trollfx-rta-arm32.so" | grep -E 'Class:|Machine:'
readelf -A "$out/VST/trollfx-rta-arm32.so" | grep 'Tag_ABI_VFP_args'
readelf -h "$out/VST/trollfx-rta-arm64.so" | grep -E 'Class:|Machine:'
echo 'No Force installation or MPC restart performed.'
