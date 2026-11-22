#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <verified plugin folder>" >&2; exit 2; }
repo="$(cd "$(dirname "$0")/.." && pwd)"; source="$(cd "$1" && pwd)"
bash "$repo/scripts/verify-force-v100.sh" "$source" >/dev/null
tmp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v100-install.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
drive="$tmp/drive"; root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
mkdir -p "$target/Service/sessions"; printf '{"uid": "TfRA", "version": "0.5.6"}\n' > "$target/plugin.json"
printf 'measurement-fixture\n' > "$target/Service/sessions/preserved.txt"
printf 'do-not-touch\n' > "$drive/MPC.settings"
bash "$repo/scripts/install-force-v100.sh" "$drive" "$source"
backup="$(cat "$drive/TROLLfx-RTA-backups/LAST_BACKUP")"
grep -F '"version": "1.0.0"' "$target/plugin.json" >/dev/null
[[ -f "$target/Service/sessions/preserved.txt" && "$(cat "$drive/MPC.settings")" == do-not-touch ]]
bash "$repo/scripts/rollback-force-v100.sh" "$drive" "$backup"
grep -F '"version": "0.5.6"' "$target/plugin.json" >/dev/null
[[ -f "$target/Service/sessions/preserved.txt" && "$(cat "$drive/MPC.settings")" == do-not-touch ]]
echo 'PASS V1.0 installation/rollback simulation preserves existing sessions and MPC settings'
