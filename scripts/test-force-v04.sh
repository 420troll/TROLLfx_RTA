#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
native_lib="${1:-$repo_dir/plugins/TrollfxRta/build/native/trollfx---vst---rta.so}"
[[ -f "$native_lib" ]] || { echo "Build native plugin first or pass its path." >&2; exit 2; }
build_dir="${TMPDIR:-/tmp}/trollfx-rta-v04-tests"
mkdir -p "$build_dir"
bash "$repo_dir/scripts/test-force-v03.sh"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"${MPC_SDK_DIR:-/tmp/mpc-plugin-builder-sdk}/framework" \
  "$repo_dir/tests/test_force_vst_host.cpp" -ldl -pthread -o "$build_dir/force-vst-host-test"
python3 "$repo_dir/tests/test_force_vst_host.py" "$build_dir/force-vst-host-test" "$native_lib" "$repo_dir/tests/fixtures/v03-state-fixture.mpcp"
python3 "$repo_dir/tests/test_force_v04_lifecycle.py" "$build_dir/force-vst-host-test" "$native_lib" "$build_dir"
cc -O2 -std=c11 -Wall -Wextra -Werror -I"$repo_dir/src" -I"$repo_dir/src/third_party/pffft" \
  "$repo_dir/src/trollfx_rta.c" "$repo_dir/src/dsp.c" \
  "$repo_dir/src/third_party/pffft/pffft.c" "$repo_dir/src/third_party/pffft/pffft_common.c" \
  -o "$build_dir/trollfx-rta-native" -lm -lpthread -ldl
python3 "$repo_dir/tests/test_analyzer_port_conflict.py" "$build_dir/trollfx-rta-native"
echo "V0.4 lifecycle and V0.2/V0.3 regressions passed."
