#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BIN="${1:-build/trollfx-rta-v02-armv7}"
HOST="${AKAI_HOST:-akai-force}"
ORIENTATION="${AKAI_ORIENTATION:-0}"
case "$ORIENTATION" in 0|90) ;; *) echo "AKAI_ORIENTATION must be 0 or 90" >&2; exit 2 ;; esac
test -x "$BIN"

# Always transfer to a new filename: BusyBox/Linux can reject replacing a running executable.
scp "$BIN" "$HOST:/tmp/trollfx-rta-v02.new"
scp dist/THIRD_PARTY_NOTICES.txt "$HOST:/tmp/trollfx-rta-v02-THIRD_PARTY_NOTICES.txt"
scp calibration/7033298.txt "$HOST:/tmp/trollfx-rta-cal-0.txt"
scp calibration/7033298_90deg.txt "$HOST:/tmp/trollfx-rta-cal-90.txt"

# Stop every known prior RTA before changing the executable. Do not touch MPC.
ssh "$HOST" 'set -e
for file in /tmp/trollfx-rta-v01.pid /tmp/trollfx-rta-v011.pid /tmp/trollfx-rta-v012.pid /tmp/trollfx-rta-v02.pid; do
  if [ -f "$file" ]; then
    kill "$(cat "$file")" 2>/dev/null || true
    rm -f "$file"
  fi
done
pkill -f "^/tmp/trollfx-rta-v0" 2>/dev/null || true
sleep 2
if netstat -ltn 2>/dev/null | grep -q "127.0.0.1:8096 "; then
  echo "ERROR: port 8096 is still occupied; inspect with BusyBox ps/netstat" >&2
  ps | grep "[t]rollfx-rta" || true
  exit 1
fi
mv -f /tmp/trollfx-rta-v02.new /tmp/trollfx-rta-v02
chmod +x /tmp/trollfx-rta-v02'

ssh "$HOST" "nohup /tmp/trollfx-rta-v02 --cal-0 /tmp/trollfx-rta-cal-0.txt --cal-90 /tmp/trollfx-rta-cal-90.txt --orientation $ORIENTATION >/tmp/trollfx-rta-v02.log 2>&1 </dev/null & echo \$! >/tmp/trollfx-rta-v02.pid"

sleep 2
ssh "$HOST" 'set -e
echo "=== V0.2 LOG ==="
cat /tmp/trollfx-rta-v02.log
if ! ps | grep -q "[t]rollfx-rta-v02"; then
  echo "ERROR: V0.2 process exited" >&2
  exit 1
fi
if ! netstat -ltn 2>/dev/null | grep -q "127.0.0.1:8096 "; then
  echo "ERROR: V0.2 did not bind port 8096" >&2
  exit 1
fi
echo "V0.2 verified running at 127.0.0.1:8096"'
echo "Rollback with: bash scripts/rollback-v02.sh"
