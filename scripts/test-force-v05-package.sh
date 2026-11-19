#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then echo "Usage: $0 <verified-v05-plugin-folder>" >&2; exit 2; fi
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
temp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-rta-v05-install.XXXXXX")"
trap 'rm -rf -- "$temp"' EXIT
root="$temp/force-media"
name="TROLLfx - VST - TROLLfx - VST - RTA"
mkdir -p "$root/VST2 Plugins/$name/VST"
cp "$1/plugin.json" "$root/VST2 Plugins/$name/"
python3 - "$root/VST2 Plugins/$name/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); p['version']='0.4.0'
json.dump(p,open(sys.argv[1],'w'))
PY
printf 'V0.4 recovery marker\n' > "$root/VST2 Plugins/$name/V04-RECOVERY-MARKER"
if ! bash "$repo_dir/scripts/install-force-v05.sh" "$root" "$1" > "$temp/install.log" 2>&1; then cat "$temp/install.log" >&2; exit 1; fi
[[ -x "$root/VST2 Plugins/$name/Service/trollfx-sweep-cli-armv7" ]] || { echo "Installed package is missing sweep CLI" >&2; exit 1; }
backup="$(cat "$root/TROLLfx-RTA-backups/LAST_BACKUP")"
[[ -f "$backup/$name/V04-RECOVERY-MARKER" ]] || { echo "Installer did not preserve the V0.4 marker" >&2; exit 1; }
if ! bash "$repo_dir/scripts/rollback-force-v05.sh" "$root" "$backup" > "$temp/rollback.log" 2>&1; then cat "$temp/rollback.log" >&2; exit 1; fi
grep -Fx 'V0.4 recovery marker' "$root/VST2 Plugins/$name/V04-RECOVERY-MARKER" >/dev/null || { echo "Rollback did not restore V0.4 marker" >&2; exit 1; }
[[ ! -e "$root/MPC.settings" ]]
echo "PASS V0.5 package install stages verified V0.5, backs up V0.4, and restores V0.4 on rollback"
