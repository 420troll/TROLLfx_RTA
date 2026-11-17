#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -z "${MPC_SDK_DIR:-}" || ! -f "$MPC_SDK_DIR/tools/build.py" ]]; then
  echo "Set MPC_SDK_DIR to the pinned InMusic MPC Plugin Builder SDK checkout." >&2; exit 2
fi
if [[ -n "${SDK_ZIG_PYTHON:-}" ]]; then
  zigcc=("$SDK_ZIG_PYTHON" -m ziglang cc)
elif command -v zig >/dev/null; then
  zigcc=(zig cc)
else
  echo "Provide SDK_ZIG_PYTHON for the pinned Zig toolchain or install zig." >&2; exit 2
fi
python3 "$repo_dir/scripts/generate-force-rta-skin.py"
args=()
[[ -z "${SDK_ZIG_PYTHON:-}" ]] || args+=(--zig "$SDK_ZIG_PYTHON")
python3 "$MPC_SDK_DIR/tools/build.py" "$repo_dir/plugins/TrollfxRta" --target all "${args[@]}"
out="$repo_dir/plugins/TrollfxRta/build/TROLLfx - VST - TROLLfx - VST - RTA"
python3 "$MPC_SDK_DIR/tools/validate.py" "$out" --strict
"$MPC_SDK_DIR/.sdk-tools/host_sim" "$repo_dir/plugins/TrollfxRta/build/native/trollfx---vst---rta.so" test --seconds 1
mkdir -p "$out/Service"
"${zigcc[@]}" -target arm-linux-gnueabihf.2.31 -mcpu=cortex_a17 -O3 -s -std=c11 -Wall -Wextra \
  -I"$repo_dir/src" -I"$repo_dir/src/third_party/pffft" \
  "$repo_dir/src/trollfx_rta.c" "$repo_dir/src/dsp.c" \
  "$repo_dir/src/third_party/pffft/pffft.c" "$repo_dir/src/third_party/pffft/pffft_common.c" \
  -o "$out/Service/trollfx-rta-v02-armv7" -lm -lpthread -ldl
mkdir -p "$out/Service/calibration"
cp "$repo_dir/calibration/7033298.txt" "$repo_dir/calibration/7033298_90deg.txt" "$out/Service/calibration/"
cp "$repo_dir/src/third_party/pffft/LICENSE.txt" "$out/Service/PFFFT-LICENSE.txt"
cp "$repo_dir/plugins/TrollfxRta/THIRD_PARTY_NOTICES.txt" "$out/THIRD_PARTY_NOTICES.txt"
cat > "$out/Service/README.txt" <<'TXT'
TROLLfx RTA V0.2 measurement service bundled for V0.4.
The VST launches this executable from the installed plugin's Service directory.
It binds only to 127.0.0.1:8096 and captures hw:U18dB,0 as S24_3LE at 48 kHz.
The service claims the port before opening ALSA, so competing launchers do not
open a second capture stream. Calibration curves for 0 and 90 degrees are in
calibration/. It retries ALSA capture after device errors. Force playback routing
is not modified. Logs are written to /tmp/trollfx-rta-v04.log.
TXT
bash "$repo_dir/scripts/test-force-v04.sh" "$repo_dir/plugins/TrollfxRta/build/native/trollfx---vst---rta.so"
package="$repo_dir/dist/force-native-v04-package"
mkdir -p "$package"
rm -rf "$package/$(basename "$out")"
cp -a "$out" "$package/"
cp "$repo_dir/docs/FORCE-NATIVE-V04.md" "$package/README-HARDWARE-TEST.md"
cp "$repo_dir/docs/FORCE-NATIVE-V04-TEST-RESULTS.md" "$package/TEST-RESULTS.md"
cp "$repo_dir/scripts/install-force-v04.sh" "$repo_dir/scripts/rollback-force-v04.sh" "$repo_dir/scripts/verify-force-v04.sh" "$package/"
bash "$repo_dir/scripts/verify-force-v04.sh" "$package/$(basename "$out")"
bash "$repo_dir/scripts/test-force-v04-package.sh" "$package/$(basename "$out")"
echo "V0.4 Force test package ready: $package"
