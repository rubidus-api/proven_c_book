#!/bin/sh
# load.c 를 -O0·-O2 로 어셈블리까지 컴파일하고 함수마다 명령 수를 센다.
# x86-64-v3 는 MOVBE 명령(읽으면서 뒤집기)을 포함하는 새 x86-64 수준이다.
# 수는 x86-64 에서만 센다. 다른 기계에서는 그렇게 알리고 멈춘다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
if [ "$(uname -m)" != "x86_64" ]; then
    echo "this demo counts x86-64 instructions; this machine is $(uname -m)"
    exit 0
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
for opt in "-O0" "-O2" "-O2 -march=x86-64-v3"; do
    # shellcheck disable=SC2086
    $cc -std=c23 -Wall -Wextra -Werror $opt -S -fno-asynchronous-unwind-tables \
        -fcf-protection=none load.c -o "$tmp/load.s"
    for fn in get_u32be_shift get_u32be_bswap get_u32le_shift; do
        # 명령 = 라벨부터 ret 까지, 들여 쓴 줄 가운데 점으로 시작하지 않는 것
        body=$(awk -v f="$fn:" '$1 == f { on = 1; next }
                 on && /^[ \t]+[a-z]/ { print $1 }
                 on && /^[ \t]+ret/ { exit }' "$tmp/load.s")
        n=$(printf "%s\n" "$body" | grep -c . || true)
        hint=""
        printf '%s\n' "$body" | grep -q '^bswap' && hint=" (uses bswap)"
        printf '%s\n' "$body" | grep -q '^movbe' && hint=" (uses movbe)"
        printf '%-20s %-16s %2d instructions%s\n' "$opt" "$fn" "$n" "$hint"
    done
done
