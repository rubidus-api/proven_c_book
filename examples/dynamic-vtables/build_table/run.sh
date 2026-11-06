#!/bin/sh
# build_table.c: 표를 짓고 검증하고 봉인하고 쓴다. 그다음 다시 돌려 봉인 뒤에
# 칸 하나를 덮어써 본다. POSIX mprotect --- Windows(VirtualProtect)는 시험하지 않았다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
$cc -std=c23 -Wall -Wextra -Werror -O2 build_table.c -o "$tmp/build_table"
"$tmp/build_table"
echo "== again, with --write-after-seal"
status=0
# 안쪽 셸이 기다리게 해서 그 셸의 「Segmentation fault」 알림을 /dev/null 로 보낸다
sh -c '"$1" --write-after-seal; exit $?' sh "$tmp/build_table" 2>/dev/null || status=$?
if [ "$status" -gt 128 ]; then
    sig=$((status - 128))
    name=$(kill -l "$sig" 2>/dev/null || echo "?")
    echo "stopped by signal $sig (SIG$name)"
else
    echo "exit status $status"
fi
