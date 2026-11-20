#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.5 plugin directory>" >&2; exit 2; }
repo="$(cd "$(dirname "$0")/.." && pwd)"; source="$(cd "$1" && pwd)"
bash "$repo/scripts/verify-force-v055.sh" "$source"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v055-package.XXXXXX")"; trap 'python3 -c '\''import shutil,sys; shutil.rmtree(sys.argv[1],ignore_errors=True)'\'' "$tmp"' EXIT
drive="$tmp/drive"; root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
mkdir -p "$target/Service/sessions"; printf '{"uid":"TfRA","version":"0.5.4"}\n' > "$target/plugin.json"
printf 'prototype-session\n' > "$target/Service/sessions/keep.txt"; printf 'keep-settings\n' > "$drive/MPC.settings"
bash "$repo/scripts/install-force-v055.sh" "$drive" "$source"
backup="$(cat "$drive/TROLLfx-RTA-backups/LAST_BACKUP")"
[[ "$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["version"])' "$target/plugin.json")" == "0.5.5" ]]
[[ -f "$target/Service/sessions/keep.txt" && "$(cat "$drive/MPC.settings")" == keep-settings ]]
bash "$repo/scripts/rollback-force-v055.sh" "$drive" "$backup"
[[ "$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["version"])' "$target/plugin.json")" == "0.5.4" ]]
[[ -f "$target/Service/sessions/keep.txt" && "$(cat "$drive/MPC.settings")" == keep-settings ]]
compgen -G "$root/.trollfx-rta-v055-failed-*" >/dev/null
echo "PASS V0.5.5 package install/rollback preserves sessions/settings and restores V0.5.4"
