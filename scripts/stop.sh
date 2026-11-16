#!/usr/bin/env bash
set -euo pipefail
ssh akai-force 'if [ -f /tmp/trollfx-rta-v012.pid ]; then kill "$(cat /tmp/trollfx-rta-v012.pid)" 2>/dev/null || true; rm -f /tmp/trollfx-rta-v012.pid; fi'
