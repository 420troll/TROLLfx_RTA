#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.1 plugin directory>" >&2; exit 2; }
repo="$(cd "$(dirname "$0")/.." && pwd)"; temp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v051-install.XXXXXX")"; trap 'rm -rf "$temp"' EXIT
root="$temp/media"; name='TROLLfx - VST - TROLLfx - VST - RTA'; mkdir -p "$root/VST2 Plugins/$name/VST"
python3 - "$root/VST2 Plugins/$name/plugin.json" <<'PY'
import json,sys
json.dump({'name':'TROLLfx - VST - RTA','uid':'TfRA','version':'0.5.0'},open(sys.argv[1],'w'))
PY
printf 'V0.5 recovery marker\n' > "$root/VST2 Plugins/$name/V05-RECOVERY-MARKER"
bash "$repo/scripts/install-force-v051.sh" "$root" "$1" > "$temp/install.log"
grep -Fx '0.5.1' <(python3 -c "import json;print(json.load(open('$root/VST2 Plugins/$name/plugin.json'))['version'])") >/dev/null
backup="$(cat "$root/TROLLfx-RTA-backups/LAST_BACKUP")"; grep -Fx 'V0.5 recovery marker' "$backup/$name/V05-RECOVERY-MARKER" >/dev/null
bash "$repo/scripts/rollback-force-v051.sh" "$root" "$backup" > "$temp/rollback.log"
grep -Fx 'V0.5 recovery marker' "$root/VST2 Plugins/$name/V05-RECOVERY-MARKER" >/dev/null
[[ ! -e "$root/MPC.settings" ]]
echo "PASS V0.5.1 installer validates/stages, backs up V0.5, and rollback restores it"
