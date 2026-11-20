#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <verified-v051-plugin-folder>" >&2; exit 2; }
repo="$(cd "$(dirname "$0")/.." && pwd)"; drive="$(cd "$1" && pwd)"; src="$(cd "$2" && pwd)"; root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'
[[ -d "$root/$name" ]] || { echo "Existing RTA plugin target not found; refusing to install." >&2; exit 1; }
bash "$repo/scripts/verify-force-v051.sh" "$src"
python3 - "$root/$name/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); assert p.get('uid')=='TfRA' and p.get('version')=='0.5.0', f"Expected V0.5.0 target, found {p.get('uid')} {p.get('version')}"
PY
stamp="$(date -u +%Y%m%dT%H%M%S%NZ)"; backup="$drive/TROLLfx-RTA-backups/$stamp"; stage="$root/.trollfx-rta-v051-stage-$stamp"; mkdir -p "$backup" "$stage"
cp -a "$src/." "$stage/"; bash "$repo/scripts/verify-force-v051.sh" "$stage"
mv -- "$root/$name" "$backup/$name"
if ! mv -- "$stage" "$root/$name"; then mv -- "$backup/$name" "$root/$name"; rm -rf -- "$stage"; echo "Install failed; V0.5.0 restored." >&2; exit 1; fi
printf '%s\n' "$backup" > "$drive/TROLLfx-RTA-backups/LAST_BACKUP"
echo "Installed V0.5.1 at $root/$name"; echo "Backup: $backup/$name"; echo "Rollback: bash '$repo/scripts/rollback-force-v051.sh' '$drive' '$backup'"
