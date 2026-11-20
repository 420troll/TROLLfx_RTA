#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 <confirmed-drive-root> <backup-directory>" >&2; exit 2; }
drive="$(cd "$1" && pwd)"; backup="$(cd "$2" && pwd)"; root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
[[ -d "$backup/$name" && -d "$target" ]] || { echo "V0.5.2 target or backup missing; refusing rollback." >&2; exit 1; }
python3 - "$backup/$name/plugin.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); assert p.get('uid')=='TfRA' and p.get('version') in ('0.5.0','0.5.1'), 'Backup is not a supported V0.5.0/V0.5.1 RTA'
PY
failed="$root/.trollfx-rta-v052-failed-$(date -u +%Y%m%dT%H%M%S%NZ)"; mv -- "$target" "$failed"; mv -- "$backup/$name" "$target"; echo "Restored V0.5.1; previous V0.5.2 retained at $failed"
