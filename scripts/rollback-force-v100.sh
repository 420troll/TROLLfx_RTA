#!/bin/sh
set -eu
[ "$#" -eq 2 ] || { echo "Usage: sh $0 <confirmed SSD root> <backup folder>" >&2; exit 2; }
drive=$(cd "$1" && pwd); backup=$(cd "$2" && pwd)
root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
[ -d "$target" ] && [ -d "$backup/$name" ] || { echo 'V1.0 target or rollback backup missing' >&2; exit 1; }
grep -F '"version": "1.0.0"' "$target/plugin.json" >/dev/null || { echo 'Active plugin is not V1.0.0' >&2; exit 1; }
(cd "$backup/$name" && sha256sum -c SHA256SUMS >/dev/null)
for maps in /proc/[0-9]*/maps; do
 [ -r "$maps" ] || continue
 if grep -F "$target/VST/trollfx-rta-arm32.so" "$maps" >/dev/null 2>&1 || grep -F "$target/VST/trollfx-rta-arm64.so" "$maps" >/dev/null 2>&1; then
   echo "RTA is loaded by ${maps#/proc/}; unload it before rollback" >&2; exit 1
 fi
done
failed="$drive/TROLLfx-RTA-backups/V100-failed-$(date -u +%Y%m%dT%H%M%SZ)"; mkdir -p "$failed"
mv "$target" "$failed/$name"
if ! mv "$backup/$name" "$target"; then mv "$failed/$name" "$target"; echo 'Rollback failed; V1.0 was restored' >&2; exit 1; fi
echo "Restored $target; V1.0 retained at $failed/$name"
