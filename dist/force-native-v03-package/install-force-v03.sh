#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <drive-root> <built-plugin-folder>" >&2
  exit 2
fi
drive_root="$(cd "$1" && pwd)"
plugin_src="$(cd "$2" && pwd)"
plugin_name="TROLLfx - VST - TROLLfx - VST - RTA"
dest_root="$drive_root/VST2 Plugins"
if [[ ! -d "$dest_root" ]]; then
  echo "No VST2 Plugins directory found at $dest_root; refusing to create an unconfirmed target." >&2
  exit 1
fi
if [[ "$(basename "$dest_root")" != "VST2 Plugins" || ! -f "$plugin_src/plugin.json" || ! -f "$plugin_src/VST/trollfx-rta-arm32.so" ]]; then
  echo "Target or plugin package did not pass validation." >&2
  exit 1
fi
backup=""
for existing_name in "$plugin_name" "TROLLfx - VST - RTA"; do
if [[ -e "$dest_root/$existing_name" ]]; then
  stamp="$(date -u +%Y%m%dT%H%M%S%NZ)"
  backup="$drive_root/TROLLfx-RTA-backups/$stamp"
  mkdir -p "$backup"
  cp -a "$dest_root/$existing_name" "$backup/$existing_name"
  printf '%s\n' "$backup" > "$drive_root/TROLLfx-RTA-backups/LAST_BACKUP"
  echo "Existing RTA backed up to: $backup/$existing_name"
  rm -rf -- "$dest_root/$existing_name"
fi
done
cp -a "$plugin_src" "$dest_root/$plugin_name"
echo "Installed only $plugin_name under: $dest_root"
echo "Rollback command: bash '$(dirname "$0")/rollback-force-v03.sh' '$drive_root' '${backup:-none}'"
