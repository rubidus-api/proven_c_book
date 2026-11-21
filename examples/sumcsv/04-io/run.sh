#!/bin/sh
# 04-io: 긴 줄 잘림 거부 · 둘째 줄 거부 · 쓰기 실패 감지 · 실패 시 stdout 0바이트를 확인한다.
set -eu
cd "$(dirname "$0")"
CC=${CC:-gcc}
$CC -std=c23 -Wall -Wextra -Werror -o sumcsv_io sumcsv_io.c

run_case() {
    name=$1
    data=$2
    arg=${3:-}
    rc=0
    if [ -n "$arg" ]; then
        printf '%b' "$data" | ./sumcsv_io "$arg" >tmp_out 2>tmp_err || rc=$?
    else
        printf '%b' "$data" | ./sumcsv_io >tmp_out 2>tmp_err || rc=$?
    fi
    ob=$(wc -c < tmp_out | tr -d ' ')
    out=$(tr -d '\n\r' < tmp_out)
    err=$(tr -d '\n\r' < tmp_err)
    printf '  %-16s rc=%d out_bytes=%-2s stdout="%s" stderr="%s"\n' "$name" "$rc" "$ob" "$out" "$err"
}

run_case "basic"      "10,20,30\n"
run_case "crlf"       " -5 , +12 \r\n"
run_case "bad-token"  "10,x,30\n"
run_case "overlong"   "1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1\n"
run_case "two-lines"  "1,2\n3,4\n"
run_case "write-fail" "10,20,30\n" "--test-write-fail"

rm -f sumcsv_io tmp_out tmp_err
