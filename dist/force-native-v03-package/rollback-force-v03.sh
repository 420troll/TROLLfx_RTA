#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <drive-root> <backup-directory-or-none>" >&2
  exit 2
fi
drive_root="$(cd "$1" && pwd)"
dest="$drive_root/VST2 Plugins/TROLLfx - VST - TROLLfx - VST - RTA"
backup="$2"
[[ -d "$drive_root/VST2 Plugins" ]] || { echo "VST2 Plugins target is missing." >&2; exit 1; }
if [[ "$backup" != "none" ]]; then
  [[ -d "$backup/TROLLfx - VST - RTA" || -d "$backup/TROLLfx - VST - TROLLfx - VST - RTA" ]] || { echo "Backup is missing." >&2; exit 1; }
fi
if [[ -e "$dest" ]]; then rm -rf -- "$dest"; fi
if [[ "$backup" != "none" ]]; then
  for saved in "$backup/TROLLfx - VST - TROLLfx - VST - RTA" "$backup/TROLLfx - VST - RTA"; do
    [[ -d "$saved" ]] && cp -a "$saved" "$drive_root/VST2 Plugins/"
  done
fi
echo "RTA rollback finished. No other plugin directories were touched."
