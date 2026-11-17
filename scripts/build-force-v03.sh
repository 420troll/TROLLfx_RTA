#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -z "${MPC_SDK_DIR:-}" || ! -f "$MPC_SDK_DIR/tools/build.py" ]]; then
  echo "Set MPC_SDK_DIR to the official inmusic-software/mpc-plugin-builder-sdk checkout." >&2
  exit 2
fi
python3 "$repo_dir/scripts/generate-force-rta-skin.py"
sdk_args=()
if [[ -n "${SDK_ZIG_PYTHON:-}" ]]; then sdk_args+=(--zig "$SDK_ZIG_PYTHON"); fi
python3 "$MPC_SDK_DIR/tools/build.py" "$repo_dir/plugins/TrollfxRta" --target all "${sdk_args[@]}"
out_dir="$repo_dir/plugins/TrollfxRta/build/TROLLfx - VST - TROLLfx - VST - RTA"
python3 "$MPC_SDK_DIR/tools/validate.py" "$out_dir" --strict
native_lib="$repo_dir/plugins/TrollfxRta/build/native/trollfx---vst---rta.so"
"$MPC_SDK_DIR/.sdk-tools/host_sim" "$native_lib" test --seconds 1
test_bin="${TMPDIR:-/tmp}/trollfx-rta-v03-host-test"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$MPC_SDK_DIR/framework" \
  "$repo_dir/tests/test_force_vst_host.cpp" -ldl -pthread -o "$test_bin"
python3 "$repo_dir/tests/test_force_vst_host.py" "$test_bin" "$native_lib"
mkdir -p "$repo_dir/dist/force-native-v03-package"
rm -rf "$repo_dir/dist/force-native-v03-package/$(basename "$out_dir")"
cp -a "$out_dir" "$repo_dir/dist/force-native-v03-package/"
cp "$repo_dir/docs/FORCE-NATIVE-V03.md" "$repo_dir/dist/force-native-v03-package/README-HARDWARE-TEST.md"
cp "$repo_dir/docs/FORCE-NATIVE-V03-TEST-RESULTS.md" "$repo_dir/dist/force-native-v03-package/TEST-RESULTS.md"
cp "$repo_dir/plugins/TrollfxRta/THIRD_PARTY_NOTICES.txt" "$repo_dir/dist/force-native-v03-package/$(basename "$out_dir")/"
cp "$repo_dir/scripts/install-force-v03.sh" "$repo_dir/dist/force-native-v03-package/"
cp "$repo_dir/scripts/rollback-force-v03.sh" "$repo_dir/dist/force-native-v03-package/"
cp "$repo_dir/scripts/verify-force-v03.sh" "$repo_dir/dist/force-native-v03-package/"
echo "Package ready: $repo_dir/dist/force-native-v03-package/$(basename "$out_dir")"
