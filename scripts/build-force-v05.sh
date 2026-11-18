#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -z "${MPC_SDK_DIR:-}" || ! -f "$MPC_SDK_DIR/tools/build.py" || -z "${SDK_ZIG_PYTHON:-}" ]]; then
  echo "Set MPC_SDK_DIR and SDK_ZIG_PYTHON for the pinned MPC SDK/Zig toolchain." >&2; exit 2
fi
zigcc=("$SDK_ZIG_PYTHON" -m ziglang cc)
zigcxx=("$SDK_ZIG_PYTHON" -m ziglang c++)
python3 "$repo_dir/scripts/generate-force-rta-skin.py"
python3 "$MPC_SDK_DIR/tools/build.py" "$repo_dir/plugins/TrollfxRta" --target all --zig "$SDK_ZIG_PYTHON"
plugin="$repo_dir/plugins/TrollfxRta/build/TROLLfx - VST - TROLLfx - VST - RTA"
python3 "$MPC_SDK_DIR/tools/validate.py" "$plugin" --strict
bash "$repo_dir/scripts/test-force-v04.sh" "$repo_dir/plugins/TrollfxRta/build/native/trollfx---vst---rta.so"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-rta-v05-vst.XXXXXX")"
trap 'rm -rf -- "$test_root"' EXIT
mkdir -p "$test_root/VST" "$test_root/Service/calibration" "$test_root/Service/sessions"
cp "$repo_dir/plugins/TrollfxRta/build/native/trollfx---vst---rta.so" "$test_root/VST/trollfx-rta-host-test.so"
cp "$repo_dir/calibration/7033298.txt" "$repo_dir/calibration/7033298_90deg.txt" "$test_root/Service/calibration/"
c++ -std=c++17 -O2 "$repo_dir/tests/make_sweep_capture.cpp" "$repo_dir/src/sweep/sweep_dsp.cpp" -o "$test_root/make-sweep-capture"
"$test_root/make-sweep-capture" "$test_root/mock-capture.wav"
python3 "$repo_dir/tests/test_force_v05_sweep_playback.py" \
  "${TMPDIR:-/tmp}/trollfx-rta-v04-tests/force-vst-host-test" \
  "$test_root/VST/trollfx-rta-host-test.so" "$test_root/mock-capture.wav"
bash "$repo_dir/scripts/test-sweep-dsp.sh"
for test in test_sweep_fixture.py test_sweep_comparison.py test_sweep_cli.py test_sweep_service_api.py; do python3 -m unittest discover -s "$repo_dir/tests" -p "$test" -v; done

mkdir -p "$plugin/Service/calibration" "$plugin/Service/sessions"
"${zigcc[@]}" -target arm-linux-gnueabihf.2.29 -mcpu=cortex_a17 -O3 -s -std=c11 -Wall -Wextra \
  -I"$repo_dir/src" -I"$repo_dir/src/third_party/pffft" \
  "$repo_dir/src/trollfx_rta.c" "$repo_dir/src/dsp.c" \
  "$repo_dir/src/third_party/pffft/pffft.c" "$repo_dir/src/third_party/pffft/pffft_common.c" \
  -o "$plugin/Service/trollfx-rta-v02-armv7" -lm -lpthread -ldl
"${zigcxx[@]}" -target arm-linux-gnueabihf.2.29 -mcpu=cortex_a17 -O3 -s -std=c++17 \
  "$repo_dir/tools/sweep_cli.cpp" "$repo_dir/src/sweep/sweep_dsp.cpp" \
  -o "$plugin/Service/trollfx-sweep-cli-armv7"
cp "$repo_dir/calibration/7033298.txt" "$repo_dir/calibration/7033298_90deg.txt" "$plugin/Service/calibration/"
cp "$repo_dir/src/third_party/pffft/LICENSE.txt" "$plugin/Service/PFFFT-LICENSE.txt"
cp "$repo_dir/plugins/TrollfxRta/THIRD_PARTY_NOTICES.txt" "$plugin/THIRD_PARTY_NOTICES.txt"
cp "$repo_dir/docs/SWEEP-LAB-V05-DSP.md" "$plugin/README-SWEEP-LAB.md"
cp "$repo_dir/docs/SWEEP-LAB-V05-HARDWARE-ACCEPTANCE.md" "$plugin/README-HARDWARE-TEST.md"
cp "$repo_dir/scripts/install-force-v05.sh" "$repo_dir/scripts/rollback-force-v05.sh" "$repo_dir/scripts/verify-force-v05.sh" "$plugin/"
cat > "$plugin/Service/README.txt" <<'TXT'
TROLLfx RTA/Sweep Lab V0.5 services. The RTA process captures the UMIK-1 at
48 kHz through the existing ALSA path and remains independent of Force playback.
The sweep CLI analyzes completed recordings only. Sweep playback is started by
the native VST START control, which arms the RTA capture service first and then
outputs the generated synchronized stereo sweep. Results are stored in
Service/sessions/. Calibration curves are bundled in calibration/.
Absolute SPL remains provisional until a reference microphone/SPL verification
is completed. The 2.7673 dB sensitivity factor is preserved as metadata and is
not treated as verified USB gain.
TXT
package="$repo_dir/dist/force-native-v05-package"
mkdir -p "$package"
rm -rf "$package/$(basename "$plugin")"
cp -a "$plugin" "$package/"
cp "$repo_dir/docs/SWEEP-LAB-V05-DSP.md" "$repo_dir/docs/SWEEP-LAB-V05-HARDWARE-ACCEPTANCE.md" "$repo_dir/docs/SWEEP-LAB-V05-TEST-RESULTS.md" "$repo_dir/docs/FORCE-NATIVE-V04.md" "$package/"
cp "$repo_dir/scripts/test-force-v05-package.sh" "$package/"
bash "$repo_dir/scripts/verify-force-v05.sh" "$package/$(basename "$plugin")"
bash "$repo_dir/scripts/test-force-v05-package.sh" "$package/$(basename "$plugin")"
(
  cd "$package"
  find "$(basename "$plugin")" -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
  sha256sum -c SHA256SUMS
)
tar -C "$repo_dir/dist" -czf "$repo_dir/dist/force-native-v05-package.tar.gz" force-native-v05-package
(cd "$repo_dir/dist" && sha256sum force-native-v05-package.tar.gz > force-native-v05-package.tar.gz.sha256)
echo "V0.5 hardware test package ready: $package"
