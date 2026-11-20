#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <verified-v055-plugin-folder>" >&2; exit 2; }
script_dir="$(cd "$(dirname "$0")" && pwd)"; drive="$(cd "$1" && pwd)"; src="$(cd "$2" && pwd)"
root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
[[ -d "$root" && -d "$target" ]] || { echo "Confirmed VST2 Plugins folder or existing RTA target not found; refusing to install." >&2; exit 1; }
verify="$script_dir/verify-force-v055.sh"; rollback="$script_dir/rollback-force-v055.sh"
[[ -x "$verify" && -x "$rollback" ]] || { echo "V0.5.5 verification/rollback scripts must be beside this installer." >&2; exit 1; }
bash "$verify" "$src"
python3 - "$target/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); assert p.get('uid')=='TfRA' and p.get('version') in ('0.5.3','0.5.4','0.5.5'), f"Expected existing TROLLfx RTA, found {p.get('uid')} {p.get('version')}"
PY
stamp="$(date -u +%Y%m%dT%H%M%S%NZ)"; backup="$drive/TROLLfx-RTA-backups/$stamp"; stage="$root/.trollfx-rta-v055-stage-$stamp"
mkdir -p "$backup" "$stage"; cp -a "$src/." "$stage/"
if [[ -d "$target/Service/sessions" ]]; then mkdir -p "$stage/Service/sessions"; cp -a "$target/Service/sessions/." "$stage/Service/sessions/"; fi
bash "$verify" "$stage"
mv -- "$target" "$backup/$name"
if ! (cd "$backup/$name" && find . -type f -print0 | sort -z | xargs -0 sha256sum > "$backup/SHA256SUMS" && sha256sum -c "$backup/SHA256SUMS"); then
    mv -- "$backup/$name" "$target"; echo "Backup verification failed; original RTA restored." >&2; exit 1
fi
if ! mv -- "$stage" "$target"; then mv -- "$backup/$name" "$target"; echo "Install failed; previous RTA restored." >&2; exit 1; fi
printf '%s\n' "$backup" > "$drive/TROLLfx-RTA-backups/LAST_BACKUP"
echo "Installed V0.5.5 at $target"; echo "Backup: $backup/$name"; echo "Rollback: bash '$rollback' '$drive' '$backup'"
echo "Do not rescan/restart MPC while a project is active. Keep Internal Audio selected."
