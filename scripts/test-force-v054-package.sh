#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 <V0.5.4 plugin directory>" >&2; exit 2; }
repo="$(cd "$(dirname "$0")/.." && pwd)"; source_folder="$(cd "$1" && pwd)"; bash "$repo/scripts/verify-force-v054.sh" "$source_folder"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/trollfx-v054-package.XXXXXX")"; trap 'rm -rf "$tmp"' EXIT
drive="$tmp/drive"; plugin_root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$plugin_root/$name"; mkdir -p "$target/Service/sessions"
printf '{"uid":"TfRA","version":"0.5.3"}\n' > "$target/plugin.json"
printf 'prototype-session\n' > "$target/Service/sessions/measurement.txt"
printf 'preserve-settings\n' > "$drive/MPC.settings"
bash "$repo/scripts/install-force-v054.sh" "$drive" "$source_folder"
backup="$(cat "$drive/TROLLfx-RTA-backups/LAST_BACKUP")"
[[ "$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["version"])' "$target/plugin.json")" == "0.5.4" ]]
[[ -f "$target/Service/sessions/measurement.txt" && "$(cat "$drive/MPC.settings")" == preserve-settings ]]
bash "$repo/scripts/rollback-force-v054.sh" "$drive" "$backup"
[[ "$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["version"])' "$target/plugin.json")" == "0.5.3" ]]
[[ -f "$target/Service/sessions/measurement.txt" ]]
compgen -G "$plugin_root/.trollfx-rta-v054-failed-*" >/dev/null
echo "PASS V0.5.4 package verification and isolated install/rollback preserve sessions and settings"
