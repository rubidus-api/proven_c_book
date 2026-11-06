#!/bin/sh
# build_table.c: build, verify, seal and use the tables; then run again and try to
# overwrite a slot after sealing. POSIX mprotect --- untested on Windows (VirtualProtect).
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
$cc -std=c23 -Wall -Wextra -Werror -O2 build_table.c -o "$tmp/build_table"
"$tmp/build_table"
echo "== again, with --write-after-seal"
status=0
# an inner shell waits for it, so its "Segmentation fault" report goes to /dev/null
sh -c '"$1" --write-after-seal; exit $?' sh "$tmp/build_table" 2>/dev/null || status=$?
if [ "$status" -gt 128 ]; then
    sig=$((status - 128))
    name=$(kill -l "$sig" 2>/dev/null || echo "?")
    echo "stopped by signal $sig (SIG$name)"
else
    echo "exit status $status"
fi
