#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <backup-directory>" >&2; exit 2; }
drive="$(cd "$1" && pwd)"; backup="$(cd "$2" && pwd)"; root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
[[ -d "$backup/$name" && -d "$target" ]] || { echo "V0.5.3 target or backup missing; refusing rollback." >&2; exit 1; }
python3 - "$target/plugin.json" "$backup/$name/plugin.json" <<'PY'
import json,sys
new,old=(json.load(open(x)) for x in sys.argv[1:]); assert new.get('uid')=='TfRA' and new.get('version')=='0.5.3', 'Installed target is not V0.5.3'; assert old.get('uid')=='TfRA' and old.get('version') in ('0.5.2','0.5.3'), 'Backup is not a supported RTA version'
PY
failed="$root/.trollfx-rta-v053-failed-$(date -u +%Y%m%dT%H%M%S%NZ)"; mv -- "$target" "$failed"; mv -- "$backup/$name" "$target"; echo "Restored backed-up RTA at $target; V0.5.3 retained at $failed"
