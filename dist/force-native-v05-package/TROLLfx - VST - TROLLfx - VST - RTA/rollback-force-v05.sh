#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 2 ]]; then echo "Usage: $0 <confirmed-drive-root> <backup-directory>" >&2; exit 2; fi
drive_root="$(cd "$1" && pwd)"
backup="$(cd "$2" && pwd)"
name="TROLLfx - VST - TROLLfx - VST - RTA"
dest="$drive_root/VST2 Plugins/$name"
saved="$backup/$name"
[[ -d "$drive_root/VST2 Plugins" && -d "$dest" && -d "$saved" ]] || { echo "Target or backup missing; refusing rollback." >&2; exit 1; }
[[ "$backup" == "$drive_root"/TROLLfx-RTA-backups/* ]] || { echo "Backup must be inside this drive's TROLLfx-RTA-backups directory." >&2; exit 1; }
python3 - "$dest/plugin.json" "$saved/plugin.json" <<'PY'
import json,sys
current=json.load(open(sys.argv[1])); saved=json.load(open(sys.argv[2]))
assert current.get('uid')=='TfRA' and current.get('version')=='0.5.0', 'Current target is not V0.5'
assert saved.get('uid')=='TfRA' and saved.get('version')=='0.4.0', 'Backup is not the V0.4 RTA'
PY
rm -rf -- "$dest"
mv -- "$saved" "$dest"
echo "Restored V0.4 plugin from $backup to $dest."
