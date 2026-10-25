#!/bin/sh
# aos_soa.c 를 최적화 없이, 그리고 최적화해서 빌드하고 한 번씩 돌린다.
# 시간은 기계와 회차마다 다르다. 프로그램은 배수만 인쇄한다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
first=1
for opt in -O0 -O2; do
    $cc -std=c23 -Wall -Wextra -Werror $opt aos_soa.c -o "$tmp/aos_soa"
    [ "$first" -eq 1 ] && "$tmp/aos_soa" | sed -n '1,2p' && first=0
    echo "== $opt"
    "$tmp/aos_soa" | sed -n '3,7p'
done
echo
echo "(ratios only: they change with the machine, the compiler and the run.)"
