#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
CC="${CC:-arm-linux-gnueabihf-gcc}"
mkdir -p build
mkdir -p dist
"$CC" -O3 -std=c11 -Wall -Wextra -march=armv7-a -mfpu=neon -mfloat-abi=hard -Isrc -Isrc/third_party/pffft src/trollfx_rta.c src/dsp.c src/third_party/pffft/pffft.c src/third_party/pffft/pffft_common.c -o build/trollfx-rta-v02-armv7 -lm -lpthread -ldl
cp build/trollfx-rta-v02-armv7 dist/trollfx-rta-v02-armv7
cp src/third_party/pffft/LICENSE.txt dist/THIRD_PARTY_NOTICES.txt
file build/trollfx-rta-v02-armv7
