#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <verified-v053-plugin-folder>" >&2; exit 2; }
script_dir="$(cd "$(dirname "$0")" && pwd)"; drive="$(cd "$1" && pwd)"; src="$(cd "$2" && pwd)"; root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
if [[ -f "$script_dir/../verify-force-v053.sh" ]]; then verify="$script_dir/../verify-force-v053.sh"; rollback="$script_dir/../rollback-force-v053.sh"; else repo="$(cd "$script_dir/.." && pwd)"; verify="$repo/scripts/verify-force-v053.sh"; rollback="$repo/scripts/rollback-force-v053.sh"; fi
[[ -d "$target" ]] || { echo "Existing RTA target not found; refusing to install." >&2; exit 1; }
bash "$verify" "$src"
python3 - "$target/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); assert p.get('uid')=='TfRA' and p.get('version') in ('0.5.2','0.5.3'), f"Expected existing V0.5.2 RTA, found {p.get('uid')} {p.get('version')}"
PY
stamp="$(date -u +%Y%m%dT%H%M%S%NZ)"; backup="$drive/TROLLfx-RTA-backups/$stamp"; stage="$root/.trollfx-rta-v053-stage-$stamp"; mkdir -p "$backup" "$stage"
cp -a "$src/." "$stage/"
if [[ -d "$target/Service/sessions" ]]; then mkdir -p "$stage/Service/sessions"; cp -a "$target/Service/sessions/." "$stage/Service/sessions/"; fi
bash "$verify" "$stage"
mv -- "$target" "$backup/$name"
if ! mv -- "$stage" "$target"; then mv -- "$backup/$name" "$target"; echo "Install failed; previous RTA restored." >&2; exit 1; fi
printf '%s\n' "$backup" > "$drive/TROLLfx-RTA-backups/LAST_BACKUP"
echo "Installed V0.5.3 at $target"; echo "Backup: $backup/$name"; echo "Rollback: bash '$rollback' '$drive' '$backup'"
echo "Do not rescan/restart MPC while a project is active."
