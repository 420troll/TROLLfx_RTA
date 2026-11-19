#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <verified-v052-plugin-folder>" >&2; exit 2; }
repo="$(cd "$(dirname "$0")/.." && pwd)"; drive="$(cd "$1" && pwd)"; src="$(cd "$2" && pwd)"; root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
[[ -d "$target" ]] || { echo "Existing RTA target not found; refusing to install." >&2; exit 1; }
bash "$repo/scripts/verify-force-v052.sh" "$src"
python3 - "$target/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); assert p.get('uid')=='TfRA' and p.get('version') in ('0.5.0','0.5.1'), f"Expected existing V0.5.0/V0.5.1 RTA, found {p.get('uid')} {p.get('version')}"
PY
stamp="$(date -u +%Y%m%dT%H%M%S%NZ)"; backup="$drive/TROLLfx-RTA-backups/$stamp"; stage="$root/.trollfx-rta-v052-stage-$stamp"; mkdir -p "$backup" "$stage"
cp -a "$src/." "$stage/"
# Carry saved captures forward unchanged; the distributed package intentionally
# has an empty sessions directory and must not overwrite a customer's sessions.
if [[ -d "$target/Service/sessions" ]]; then mkdir -p "$stage/Service/sessions"; cp -a "$target/Service/sessions/." "$stage/Service/sessions/"; fi
bash "$repo/scripts/verify-force-v052.sh" "$stage"
mv -- "$target" "$backup/$name"
if ! mv -- "$stage" "$target"; then mv -- "$backup/$name" "$target"; echo "Install failed; V0.5.1 restored." >&2; exit 1; fi
printf '%s\n' "$backup" > "$drive/TROLLfx-RTA-backups/LAST_BACKUP"
echo "Installed V0.5.2 at $target"; echo "Backup: $backup/$name"; echo "Rollback: bash '$repo/scripts/rollback-force-v052.sh' '$drive' '$backup'"
