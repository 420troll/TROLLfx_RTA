#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
CC="${CC:-arm-linux-gnueabihf-gcc}"
"$CC" -O2 -std=c11 -Wall -Wextra src/trollfx_rta.c -o /tmp/trollfx-rta-v012 -lm -lpthread -ldl
ssh akai-force 'if [ -f /tmp/trollfx-rta-v012.pid ]; then kill "$(cat /tmp/trollfx-rta-v012.pid)" 2>/dev/null || true; fi; pkill -f "^/tmp/trollfx-rta-v012" 2>/dev/null || true'
scp /tmp/trollfx-rta-v012 akai-force:/tmp/trollfx-rta-v012
ssh akai-force 'nohup /tmp/trollfx-rta-v012 >/tmp/trollfx-rta-v012.log 2>&1 </dev/null & echo $! >/tmp/trollfx-rta-v012.pid; sleep 1; cat /tmp/trollfx-rta-v012.log'
echo 'Now: ssh -N -L 8096:127.0.0.1:8096 akai-force'
