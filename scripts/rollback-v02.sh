#!/usr/bin/env bash
set -euo pipefail
HOST="${AKAI_HOST:-akai-force}"
ssh "$HOST" 'if [ -f /tmp/trollfx-rta-v02.pid ]; then kill "$(cat /tmp/trollfx-rta-v02.pid)" 2>/dev/null || true; rm -f /tmp/trollfx-rta-v02.pid; fi; pkill -f "^/tmp/trollfx-rta-v02" 2>/dev/null || true; if [ -x /tmp/trollfx-rta-v012 ]; then nohup /tmp/trollfx-rta-v012 >/tmp/trollfx-rta-v012.log 2>&1 </dev/null & echo $! >/tmp/trollfx-rta-v012.pid; fi'
echo "V0.2 stopped; the retained V0.1.2 executable was restarted if present."
