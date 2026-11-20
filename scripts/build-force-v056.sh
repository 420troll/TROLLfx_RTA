#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
sdk="${MPC_SDK_DIR:-/tmp/mpc-plugin-builder-sdk}"
if [[ ! -f "$sdk/tools/build.py" || -z "${SDK_ZIG_PYTHON:-}" ]]; then
    echo "Set MPC_SDK_DIR and SDK_ZIG_PYTHON to the pinned MPC SDK and Zig Python." >&2; exit 2
fi
base="$repo/dist/force-native-v055-package/TROLLfx - VST - TROLLfx - VST - RTA"
[[ -d "$base/Service" ]] || { echo "V0.5.5 package with the unchanged analyzer service is required at $base" >&2; exit 2; }
python3 -m py_compile "$repo/scripts/generate-force-rta-skin.py" "$repo/tests/test_force_v05_sweep_playback.py" "$repo/tests/test_force_v055_touch_skin.py" "$repo/tests/test_force_v056_skin.py"
python3 "$repo/scripts/generate-force-rta-skin.py"
# The SDK copies ancillary files into its output directory; clear old builds so
# an evaluation package cannot inherit stale V0.5.4/V0.5.5 scripts or assets.
python3 - "$repo/plugins/TrollfxRta/build" <<'PY'
import shutil,sys
shutil.rmtree(sys.argv[1],ignore_errors=True)
PY
python3 "$sdk/tools/build.py" "$repo/plugins/TrollfxRta" --target all --zig "$SDK_ZIG_PYTHON"
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
    echo "pyfar comparison skipped: install requirements-sweep-reference.txt to enable it"
fi

tmp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v056-build.XXXXXX")"
trap 'python3 -c '\''import shutil,sys; shutil.rmtree(sys.argv[1],ignore_errors=True)'\'' "$tmp"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$sdk/framework" "$repo/tests/test_force_vst_host.cpp" -ldl -pthread -o "$tmp/host-test"
c++ -std=c++17 -O2 "$repo/tests/make_sweep_capture.cpp" "$repo/src/sweep/sweep_dsp.cpp" -o "$tmp/make-capture"
"$tmp/make-capture" "$tmp/capture.wav"
for mode in playback cancel; do
    test_root="$tmp/$mode"; mkdir -p "$test_root/VST" "$test_root/Service/calibration" "$test_root/Service/sessions"
    cp "$native" "$test_root/VST/host.so"
    cp "$repo/calibration/7033298.txt" "$repo/calibration/7033298_90deg.txt" "$test_root/Service/calibration/"
    if [[ "$mode" == cancel ]]; then
        python3 "$repo/tests/test_force_v05_sweep_playback.py" "$tmp/host-test" "$test_root/VST/host.so" "$tmp/capture.wav" --sweep-cancel
    else
        python3 "$repo/tests/test_force_v05_sweep_playback.py" "$tmp/host-test" "$test_root/VST/host.so" "$tmp/capture.wav"
    fi
done

package="$repo/dist/force-native-v056-package"
python3 - "$package" <<'PY'
import shutil,sys
from pathlib import Path
p=Path(sys.argv[1])
if p.exists(): shutil.rmtree(p)
p.mkdir(parents=True)
PY
name="$(basename "$plugin")"; out="$package/$name"
cp -a "$plugin" "$out"
cp -a "$base/Service/." "$out/Service/"
mkdir -p "$out/Service/sessions"
cat > "$out/Service/README.txt" <<'EOF'
TROLLfx RTA V0.5.6. Live RTA, analyzer service, microphone capture and sweep analysis are unchanged from V0.5.5. Sweep result displays use fixed 1/6-octave smoothing with one shared automatic dB range covering the complete valid smoothed response plus modest headroom. Original capture WAV, reference WAV, impulse WAV, raw response CSV and phase metadata are preserved. Saved measurement selection re-reads raw response and does not rewrite stored sessions. The graph uses 256 logarithmically sampled points connected by 255 native SDK Image filmstrip line segments. The reduced atlas is 4 x 61,568 pixels; host validator acceptance is not physical Force verification. Keep playback set to Internal Audio. See FORCE-NATIVE-V056-README.md and FORCE-NATIVE-V056-VALIDATION.md.
EOF
cp "$repo/docs/FORCE-NATIVE-V056-README.md" "$repo/docs/FORCE-NATIVE-V056-VALIDATION.md" "$repo/docs/FORCE-NATIVE-V04.md" "$out/"
cp "$repo/plugins/TrollfxRta/THIRD_PARTY_NOTICES.txt" "$out/"
cp "$repo/docs/FORCE-NATIVE-V056-README.md" "$repo/docs/FORCE-NATIVE-V056-VALIDATION.md" "$package/"
cp "$repo/scripts/install-force-v056.sh" "$repo/scripts/rollback-force-v056.sh" "$repo/scripts/verify-force-v056.sh" "$out/"
cp "$repo/scripts/install-force-v056.sh" "$repo/scripts/rollback-force-v056.sh" "$repo/scripts/verify-force-v056.sh" "$repo/scripts/test-force-v056-package.sh" "$package/"
(
    cd "$out"; find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
bash "$repo/scripts/verify-force-v056.sh" "$out"
bash "$repo/scripts/test-force-v056-package.sh" "$out"
(
    cd "$package"; find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
    sha256sum -c SHA256SUMS
)
tar -C "$repo/dist" -czf "$repo/dist/force-native-v056-package.tar.gz" force-native-v056-package
(cd "$repo/dist" && sha256sum force-native-v056-package.tar.gz > force-native-v056-package.tar.gz.sha256)
printf '\nV0.5.6 package: %s\n' "$package"
printf 'Package SHA256: '; sha256sum "$repo/dist/force-native-v056-package.tar.gz" | cut -d' ' -f1
printf 'ARM32 VST SHA256: '; sha256sum "$out/VST/trollfx-rta-arm32.so" | cut -d' ' -f1
readelf -h "$out/VST/trollfx-rta-arm32.so" | grep -E 'Class:|Machine:'
readelf -A "$out/VST/trollfx-rta-arm32.so" | grep 'Tag_ABI_VFP_args'
echo "No hardware install or MPC restart performed."
