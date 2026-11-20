#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
${CXX:-g++} -O2 -std=c++17 -Wall -Wextra -Wpedantic tests/test_sweep_dsp.cpp src/sweep/sweep_dsp.cpp -o build/test-sweep-dsp
./build/test-sweep-dsp
${CXX:-g++} -O2 -std=c++17 -Wall -Wextra -Wpedantic tests/test_sweep_display.cpp src/sweep/sweep_display.cpp src/sweep/sweep_dsp.cpp -o build/test-sweep-display
./build/test-sweep-display
${CXX:-g++} -O2 -std=c++17 -Wall -Wextra -Wpedantic tools/sweep_cli.cpp src/sweep/sweep_dsp.cpp -o build/trollfx-sweep-cli
./build/trollfx-sweep-cli --version
python3 -m unittest discover -s tests -p test_sweep_cli.py -v
if python3 -c 'import pyfar' >/dev/null 2>&1; then
  python3 -m unittest discover -s tests -p test_sweep_pyfar.py -v
else
  echo "pyfar oracle skipped (install requirements-sweep-reference.txt to enable)"
fi
