#!/bin/sh
# 정책 없는 sumcsv 초안이 엉뚱한 입력을 어떻게 통과시키는지 보인다.
set -eu
cd "$(dirname "$0")"
CC=${CC:-gcc}
$CC -std=c23 -Wall -Wextra -Werror -o sumcsv_naive sumcsv_naive.c

run_case() {
    label=$1
    data=$2
    rc=0
    out=$(printf '%b' "$data" | ./sumcsv_naive) || rc=$?
    printf '  %-14s -> stdout=%-6s exit=%d\n' "$label" "$out" "$rc"
}

run_case '"10,20,30"' '10,20,30\n'
run_case '"10,x,30"'  '10,x,30\n'
run_case '"10,20,"'   '10,20,\n'
run_case '"10,,30"'   '10,,30\n'
run_case '"" (EOF)'   ''
run_case '"   "'      '   \n'

rm -f sumcsv_naive
