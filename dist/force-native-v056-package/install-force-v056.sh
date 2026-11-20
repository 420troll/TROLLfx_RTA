#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <verified-v056-plugin-folder>" >&2; exit 2; }
script_dir="$(cd "$(dirname "$0")" && pwd)"; drive="$(cd "$1" && pwd)"; src="$(cd "$2" && pwd)"
root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
[[ -d "$root" && -d "$target" ]] || { echo "Confirmed VST2 Plugins folder or existing RTA target not found; refusing to install." >&2; exit 1; }
verify="$script_dir/verify-force-v056.sh"; rollback="$script_dir/rollback-force-v056.sh"
[[ -x "$verify" && -x "$rollback" ]] || { echo "V0.5.6 verification/rollback scripts must be beside this installer." >&2; exit 1; }
bash "$verify" "$src"
python3 - "$target/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); assert p.get('uid')=='TfRA' and p.get('version') in ('0.5.3','0.5.4','0.5.5','0.5.6'), f"Expected existing TROLLfx RTA, found {p.get('uid')} {p.get('version')}"
PY
# Never replace a VST binary that an MPC process currently has mapped. This
# check is read-only and intentionally does not stop or restart MPC.
for maps in /proc/[0-9]*/maps; do
    [[ -r "$maps" ]] || continue
    if grep -Fq "$target/VST/trollfx-rta-arm32.so" "$maps" 2>/dev/null || grep -Fq "$target/VST/trollfx-rta-arm64.so" "$maps" 2>/dev/null; then
        echo "RTA VST is actively mapped by ${maps#/proc/}; unload the plugin and retry. MPC was not changed." >&2
        exit 1
    fi
done
stamp="$(date -u +%Y%m%dT%H%M%S%NZ)"; backup="$drive/TROLLfx-RTA-backups/$stamp"; stage="$root/.trollfx-rta-v056-stage-$stamp"
mkdir -p "$backup" "$stage"; cp -a "$src/." "$stage/"
if [[ -d "$target/Service/sessions" ]]; then mkdir -p "$stage/Service/sessions"; cp -a "$target/Service/sessions/." "$stage/Service/sessions/"; fi
bash "$verify" "$stage"
mv -- "$target" "$backup/$name"
if ! (cd "$backup/$name" && find . -type f -print0 | sort -z | xargs -0 sha256sum > "$backup/SHA256SUMS" && sha256sum -c "$backup/SHA256SUMS"); then
    mv -- "$backup/$name" "$target"; echo "Backup verification failed; original RTA restored." >&2; exit 1
fi
if ! mv -- "$stage" "$target"; then mv -- "$backup/$name" "$target"; echo "Install failed; previous RTA restored." >&2; exit 1; fi
printf '%s\n' "$backup" > "$drive/TROLLfx-RTA-backups/LAST_BACKUP"
echo "Installed V0.5.6 at $target"; echo "Backup: $backup/$name"; echo "Rollback: bash '$rollback' '$drive' '$backup'"
echo "Do not rescan/restart MPC while a project is active. Keep Internal Audio selected."
