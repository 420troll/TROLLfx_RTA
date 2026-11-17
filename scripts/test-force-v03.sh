#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${TMPDIR:-/tmp}/trollfx-rta-v03-tests"
mkdir -p "$build_dir"
c++ -std=c++17 -Wall -Wextra -Werror -O2 \
  "$repo_dir/tests/test_spectrum_json.cpp" \
  "$repo_dir/plugins/TrollfxRta/src/spectrum_json.cpp" \
  -o "$build_dir/test_spectrum_json" -lm
"$build_dir/test_spectrum_json"
bash "$repo_dir/scripts/test-force-display.sh"
bash "$repo_dir/scripts/run-tests.sh"
