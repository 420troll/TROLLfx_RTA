#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then echo "Usage: $0 <verified-plugin-folder>" >&2; exit 2; fi
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
temp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-rta-v04-install.XXXXXX")"
trap 'rm -rf -- "$temp"' EXIT
root="$temp/force-media"
name="TROLLfx - VST - TROLLfx - VST - RTA"
mkdir -p "$root/VST2 Plugins/$name"
printf 'V0.3 recovery marker\n' > "$root/VST2 Plugins/$name/V03-RECOVERY-MARKER"
bash "$repo_dir/scripts/install-force-v04.sh" "$root" "$1" > "$temp/install.log"
[[ -x "$root/VST2 Plugins/$name/Service/trollfx-rta-v02-armv7" ]]
backup="$(cat "$root/TROLLfx-RTA-backups/LAST_BACKUP")"
[[ -f "$backup/$name/V03-RECOVERY-MARKER" ]]
bash "$repo_dir/scripts/rollback-force-v04.sh" "$root" "$backup" > "$temp/rollback.log"
grep -Fx 'V0.3 recovery marker' "$root/VST2 Plugins/$name/V03-RECOVERY-MARKER" >/dev/null
[[ ! -e "$root/MPC.settings" ]]
echo "PASS package install stages V0.4, backs up V0.3, and restores it on rollback"
