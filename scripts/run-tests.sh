#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
${CC:-gcc} -O2 -std=c11 -Wall -Wextra -Isrc tests/test_dsp.c src/dsp.c src/third_party/pffft/pffft.c src/third_party/pffft/pffft_common.c -Isrc/third_party/pffft -o build/test-dsp -lm
./build/test-dsp
${CC:-gcc} -O2 -std=c11 -Wall -Wextra -Isrc tests/benchmark.c src/dsp.c src/third_party/pffft/pffft.c src/third_party/pffft/pffft_common.c -Isrc/third_party/pffft -o build/benchmark-dsp -lm
./build/benchmark-dsp
