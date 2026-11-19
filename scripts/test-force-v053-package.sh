#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.3 plugin directory>" >&2; exit 2; }
repo="$(cd "$(dirname "$0")/.." && pwd)"; tmp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v053-install.XXXXXX")"; trap 'rm -rf "$tmp"' EXIT
root="$tmp/media"; name='TROLLfx - VST - TROLLfx - VST - RTA'; old="$root/VST2 Plugins/$name"; mkdir -p "$old/VST" "$old/Service/sessions/sweep-preserve"
python3 - "$old/plugin.json" <<'PY'
import json,sys
json.dump({'name':'TROLLfx - VST - RTA','uid':'TfRA','version':'0.5.2'},open(sys.argv[1],'w'))
PY
printf 'capture stays byte-identical\n' > "$old/Service/sessions/sweep-preserve/capture.wav"
printf 'settings stay untouched\n' > "$root/MPC.settings"; settings_before="$(sha256sum "$root/MPC.settings")"
bash "$1/install-force-v053.sh" "$root" "$1" > "$tmp/install.log"
grep -F '0.5.3' "$root/VST2 Plugins/$name/plugin.json" >/dev/null
[[ "$(cat "$root/VST2 Plugins/$name/Service/sessions/sweep-preserve/capture.wav")" == 'capture stays byte-identical' ]]
[[ "$(sha256sum "$root/MPC.settings")" == "$settings_before" ]]
backup="$(cat "$root/TROLLfx-RTA-backups/LAST_BACKUP")"; bash "$1/rollback-force-v053.sh" "$root" "$backup" > "$tmp/rollback.log"
grep -F '0.5.2' "$root/VST2 Plugins/$name/plugin.json" >/dev/null
[[ "$(cat "$root/VST2 Plugins/$name/Service/sessions/sweep-preserve/capture.wav")" == 'capture stays byte-identical' ]]
[[ "$(sha256sum "$root/MPC.settings")" == "$settings_before" ]]
echo "PASS V0.5.3 package install/rollback preserves session files and MPC.settings"
