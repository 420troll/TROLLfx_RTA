#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.2 plugin directory>" >&2; exit 2; }
repo="$(cd "$(dirname "$0")/.." && pwd)"; temp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v052-install.XXXXXX")"; trap 'rm -rf "$temp"' EXIT
root="$temp/media"; name='TROLLfx - VST - TROLLfx - VST - RTA'; old="$root/VST2 Plugins/$name"; mkdir -p "$old/VST" "$old/Service/sessions"
python3 - "$old/plugin.json" <<'PY'
import json,sys
json.dump({'name':'TROLLfx - VST - RTA','uid':'TfRA','version':'0.5.1'},open(sys.argv[1],'w'))
PY
printf 'V0.5.1 recovery marker\n' > "$old/V051-RECOVERY-MARKER"
mkdir -p "$old/Service/sessions/sweep-keep"; printf 'raw capture preserve\n' > "$old/Service/sessions/sweep-keep/capture.wav"
bash "$repo/scripts/install-force-v052.sh" "$root" "$1" > "$temp/install.log"
grep -Fx 'V0.5.1 recovery marker' "$(cat "$root/TROLLfx-RTA-backups/LAST_BACKUP")/$name/V051-RECOVERY-MARKER" >/dev/null
[[ "$(cat "$root/VST2 Plugins/$name/Service/sessions/sweep-keep/capture.wav")" == 'raw capture preserve' ]]
grep -F '0.5.2' "$root/VST2 Plugins/$name/plugin.json" >/dev/null
backup="$(cat "$root/TROLLfx-RTA-backups/LAST_BACKUP")"; bash "$repo/scripts/rollback-force-v052.sh" "$root" "$backup" > "$temp/rollback.log"
grep -F '0.5.1' "$root/VST2 Plugins/$name/plugin.json" >/dev/null
[[ "$(cat "$root/VST2 Plugins/$name/Service/sessions/sweep-keep/capture.wav")" == 'raw capture preserve' ]]
[[ ! -e "$root/MPC.settings" ]]
python3 - "$root/VST2 Plugins/$name/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1]));p['version']='0.5.0';json.dump(p,open(sys.argv[1],'w'))
PY
bash "$repo/scripts/install-force-v052.sh" "$root" "$1" > "$temp/install-v050.log"
backup="$(cat "$root/TROLLfx-RTA-backups/LAST_BACKUP")"; bash "$repo/scripts/rollback-force-v052.sh" "$root" "$backup" > "$temp/rollback-v050.log"
grep -F '0.5.0' "$root/VST2 Plugins/$name/plugin.json" >/dev/null
echo "PASS V0.5.2 package install/rollback, existing-session preservation and MPC.settings non-modification"
