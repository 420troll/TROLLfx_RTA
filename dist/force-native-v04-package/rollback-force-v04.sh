#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 2 ]]; then echo "Usage: $0 <confirmed-drive-root> <backup-directory>" >&2; exit 2; fi
drive_root="$(cd "$1" && pwd)"
backup="$(cd "$2" && pwd)"
name="TROLLfx - VST - TROLLfx - VST - RTA"
dest="$drive_root/VST2 Plugins/$name"
saved="$backup/$name"
[[ -d "$drive_root/VST2 Plugins" && -d "$saved" ]] || { echo "Target or V0.3 backup missing; refusing rollback." >&2; exit 1; }
[[ "$backup" == "$drive_root"/TROLLfx-RTA-backups/* ]] || { echo "Backup must be inside this drive's TROLLfx-RTA-backups directory." >&2; exit 1; }
rm -rf -- "$dest"
mv -- "$saved" "$dest"
echo "Restored V0.3 plugin from $backup to $dest."
