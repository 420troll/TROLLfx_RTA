#!/bin/sh
# BusyBox-compatible safe upgrade; does not scan, restart MPC, or touch routing.
set -eu
[ "$#" -eq 2 ] || { echo "Usage: sh $0 <confirmed SSD root> <verified V1.0.0 plugin folder>" >&2; exit 2; }
drive=$(cd "$1" && pwd); src=$(cd "$2" && pwd)
root="$drive/VST2 Plugins"; name='TROLLfx - VST - TROLLfx - VST - RTA'; target="$root/$name"
script_dir=$(cd "$(dirname "$0")" && pwd)
[ -d "$root" ] && [ -d "$target" ] || { echo 'SSD VST2 Plugins or existing RTA folder not found' >&2; exit 1; }
sh "$script_dir/verify-force-v100.sh" "$src" >/dev/null
grep -F '"uid": "TfRA"' "$target/plugin.json" >/dev/null || { echo 'Existing folder is not TROLLfx RTA' >&2; exit 1; }
grep -E '"version": "(0\.5\.6|1\.0\.0)"' "$target/plugin.json" >/dev/null || { echo 'Expected V0.5.6 or V1.0.0 target; refusing replacement' >&2; exit 1; }
for maps in /proc/[0-9]*/maps; do
 [ -r "$maps" ] || continue
 if grep -F "$target/VST/trollfx-rta-arm32.so" "$maps" >/dev/null 2>&1 || grep -F "$target/VST/trollfx-rta-arm64.so" "$maps" >/dev/null 2>&1; then
   echo "RTA is loaded by ${maps#/proc/}; unload the effect before installing. MPC was not changed." >&2; exit 1
 fi
done
stamp=$(date -u +%Y%m%dT%H%M%SZ); backup="$drive/TROLLfx-RTA-backups/V056-before-V100-$stamp"; stage="$root/.trollfx-rta-v100-stage-$stamp"
mkdir -p "$backup" "$stage"; cp -R "$src/." "$stage/"
if [ -d "$target/Service/sessions" ]; then mkdir -p "$stage/Service/sessions"; cp -R "$target/Service/sessions/." "$stage/Service/sessions/"; fi
sh "$script_dir/verify-force-v100.sh" "$stage" >/dev/null
for maps in /proc/[0-9]*/maps; do
 [ -r "$maps" ] || continue
 if grep -F "$target/VST/trollfx-rta-arm32.so" "$maps" >/dev/null 2>&1 || grep -F "$target/VST/trollfx-rta-arm64.so" "$maps" >/dev/null 2>&1; then
   rm -rf "$stage"; echo 'RTA became loaded during staging; refusing replacement' >&2; exit 1
 fi
done
mv "$target" "$backup/$name"
if ! (cd "$backup/$name" && find . -type f ! -name SHA256SUMS -exec sha256sum {} \; > SHA256SUMS && sha256sum -c SHA256SUMS >/dev/null); then
 mv "$backup/$name" "$target"; rm -rf "$stage"; echo 'Backup verification failed; old plugin restored' >&2; exit 1
fi
if ! mv "$stage" "$target"; then mv "$backup/$name" "$target"; echo 'Install failed; old plugin restored' >&2; exit 1; fi
printf '%s\n' "$backup" > "$drive/TROLLfx-RTA-backups/LAST_BACKUP"
echo "Installed V1.0.0 at $target"
echo "Rollback: sh '$script_dir/rollback-force-v100.sh' '$drive' '$backup'"
echo 'MPC was not restarted; Internal Audio and other plugins were not changed.'
