#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <backup-directory>" >&2; exit 2; }
drive="$(cd "$1" && pwd)"; backup="$(cd "$2" && pwd)"; root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'
[[ -d "$backup/$name" && -d "$root/$name" ]] || { echo "V0.5.1 target or backup missing; refusing rollback." >&2; exit 1; }
python3 - "$backup/$name/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); assert p.get('uid')=='TfRA' and p.get('version')=='0.5.0', 'Backup is not V0.5.0'
PY
failed="$root/.trollfx-rta-v051-failed-$(date -u +%Y%m%dT%H%M%S%NZ)"; mv -- "$root/$name" "$failed"; mv -- "$backup/$name" "$root/$name"; echo "Restored V0.5.0; previous V0.5.1 retained at $failed"
