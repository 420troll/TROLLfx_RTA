#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BIN="${1:-build/trollfx-rta-v02-armv7}"
HOST="${AKAI_HOST:-akai-force}"
ORIENTATION="${AKAI_ORIENTATION:-0}"
case "$ORIENTATION" in 0|90) ;; *) echo "AKAI_ORIENTATION must be 0 or 90" >&2; exit 2 ;; esac
test -x "$BIN"
scp "$BIN" "$HOST:/tmp/trollfx-rta-v02"
scp dist/THIRD_PARTY_NOTICES.txt "$HOST:/tmp/trollfx-rta-v02-THIRD_PARTY_NOTICES.txt"
scp calibration/7033298.txt "$HOST:/tmp/trollfx-rta-cal-0.txt"
scp calibration/7033298_90deg.txt "$HOST:/tmp/trollfx-rta-cal-90.txt"
ssh "$HOST" "pkill -f '^/tmp/trollfx-rta-v012' 2>/dev/null || true; pkill -f '^/tmp/trollfx-rta-v02' 2>/dev/null || true; nohup /tmp/trollfx-rta-v02 --cal-0 /tmp/trollfx-rta-cal-0.txt --cal-90 /tmp/trollfx-rta-cal-90.txt --orientation $ORIENTATION >/tmp/trollfx-rta-v02.log 2>&1 </dev/null & echo \$! >/tmp/trollfx-rta-v02.pid; sleep 1; cat /tmp/trollfx-rta-v02.log"
echo "V0.2 test instance is running. Roll back with: scripts/rollback-v02.sh"
