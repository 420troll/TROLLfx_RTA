#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 2 ]]; then echo "Usage: $0 <confirmed-drive-root> <built-v04-plugin-folder>" >&2; exit 2; fi
drive_root="$(cd "$1" && pwd)"
plugin_src="$(cd "$2" && pwd)"
dest_root="$drive_root/VST2 Plugins"
plugin_name="TROLLfx - VST - TROLLfx - VST - RTA"
[[ -d "$dest_root" ]] || { echo "VST2 Plugins directory not found at $dest_root; refusing to create a target." >&2; exit 1; }
[[ -d "$dest_root/$plugin_name" ]] || { echo "Expected installed V0.3 RTA at $dest_root/$plugin_name; refusing to replace an unknown plugin." >&2; exit 1; }
"$(dirname "$0")/verify-force-v04.sh" "$plugin_src"
stamp="$(date -u +%Y%m%dT%H%M%S%NZ)"
backup="$drive_root/TROLLfx-RTA-backups/$stamp"
stage="$dest_root/.trollfx-rta-v04-stage-$stamp"
mkdir -p "$backup" "$stage"
cp -a "$plugin_src/." "$stage/"
if ! [[ -x "$stage/Service/trollfx-rta-v02-armv7" ]]; then rm -rf -- "$stage"; echo "Staged package validation failed." >&2; exit 1; fi
mv -- "$dest_root/$plugin_name" "$backup/$plugin_name"
if ! mv -- "$stage" "$dest_root/$plugin_name"; then mv -- "$backup/$plugin_name" "$dest_root/$plugin_name"; rm -rf -- "$stage"; echo "Install failed; V0.3 restored." >&2; exit 1; fi
printf '%s\n' "$backup" > "$drive_root/TROLLfx-RTA-backups/LAST_BACKUP"
echo "Installed V0.4 at $dest_root/$plugin_name"
echo "V0.3 backup: $backup/$plugin_name"
echo "Rollback: bash '$(dirname "$0")/rollback-force-v04.sh' '$drive_root' '$backup'"
