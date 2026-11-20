#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <backup-directory>" >&2; exit 2; }
drive="$(cd "$1" && pwd)"; backup="$(cd "$2" && pwd)"; root="$drive/VST2 Plugins"
name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
[[ -d "$backup/$name" && -d "$target" ]] || { echo "V0.5.6 target or backup missing; refusing rollback." >&2; exit 1; }
if [[ -f "$backup/SHA256SUMS" ]]; then (cd "$backup/$name" && sha256sum -c "$backup/SHA256SUMS"); fi
python3 - "$target/plugin.json" "$backup/$name/plugin.json" <<'PY'
import json,sys
new,old=(json.load(open(x)) for x in sys.argv[1:]); assert new.get('uid')=='TfRA' and new.get('version')=='0.5.6', 'Installed target is not V0.5.6'; assert old.get('uid')=='TfRA' and old.get('version') in ('0.5.3','0.5.4','0.5.5','0.5.6'), 'Backup is not a supported RTA version'
PY
for maps in /proc/[0-9]*/maps; do
    [[ -r "$maps" ]] || continue
    if grep -Fq "$target/VST/trollfx-rta-arm32.so" "$maps" 2>/dev/null || grep -Fq "$target/VST/trollfx-rta-arm64.so" "$maps" 2>/dev/null; then
        echo "RTA VST is actively mapped by ${maps#/proc/}; unload the plugin before rollback." >&2
        exit 1
    fi
done
failed="$root/.trollfx-rta-v056-failed-$(date -u +%Y%m%dT%H%M%S%NZ)"; mv -- "$target" "$failed"; mv -- "$backup/$name" "$target"
echo "Restored backed-up RTA at $target; V0.5.6 retained at $failed"
