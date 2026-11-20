#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 2 ]]; then echo "Usage: $0 <confirmed-drive-root> <verified-v05-plugin-folder>" >&2; exit 2; fi
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
drive_root="$(cd "$1" && pwd)"
plugin_src="$(cd "$2" && pwd)"
dest_root="$drive_root/VST2 Plugins"
name="TROLLfx - VST - TROLLfx - VST - RTA"
[[ -d "$dest_root" && -d "$dest_root/$name" ]] || { echo "VST2 Plugins and existing RTA plugin must exist; refusing unknown target." >&2; exit 1; }
bash "$repo_dir/scripts/verify-force-v05.sh" "$plugin_src"
python3 - "$dest_root/$name/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1]))
assert p.get('uid')=='TfRA' and p.get('version')=='0.4.0', f"Expected installed V0.4 UID TfRA, found {p.get('uid')} {p.get('version')}"
print('Existing V0.4 RTA identified; safe replacement target confirmed.')
PY
stamp="$(date -u +%Y%m%dT%H%M%S%NZ)"
backup="$drive_root/TROLLfx-RTA-backups/$stamp"
stage="$dest_root/.trollfx-rta-v05-stage-$stamp"
mkdir -p "$backup" "$stage"
cp -a "$plugin_src/." "$stage/"
bash "$repo_dir/scripts/verify-force-v05.sh" "$stage"
mv -- "$dest_root/$name" "$backup/$name"
if ! mv -- "$stage" "$dest_root/$name"; then mv -- "$backup/$name" "$dest_root/$name"; rm -rf -- "$stage"; echo "Install failed; V0.4 restored." >&2; exit 1; fi
printf '%s\n' "$backup" > "$drive_root/TROLLfx-RTA-backups/LAST_BACKUP"
echo "Installed V0.5 at $dest_root/$name"
echo "V0.4 backup: $backup/$name"
echo "Rollback: bash '$repo_dir/scripts/rollback-force-v05.sh' '$drive_root' '$backup'"
