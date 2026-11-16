#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
${CC:-gcc} -std=c11 -O2 -Wall -Wextra -Werror -Isrc/force \
  src/force/force_rta_display.c tests/test_force_rta_display.c \
  -lm -o build/test-force-display
./build/test-force-display
