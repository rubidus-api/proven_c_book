#!/bin/sh
# 05-project: 여러 파일 빌드 + 골든 시험 8건(stdout/stderr/종료코드) + 고치기 전 회귀 시험 실패 확인.
set -eu
cd "$(dirname "$0")"
CC=${CC:-gcc}
make -s CC="$CC" clean all
mkdir -p tests/tmp

fail=0
for input in tests/in/*.txt; do
    name=$(basename "$input" .txt)
    rc=0
    ./sumcsv < "$input" > "tests/tmp/$name.out" 2> "tests/tmp/$name.err" || rc=$?
    printf '%s\n' "$rc" > "tests/tmp/$name.rc"
    if cmp -s "tests/expected/$name.out" "tests/tmp/$name.out" &&
       cmp -s "tests/expected/$name.err" "tests/tmp/$name.err" &&
       cmp -s "tests/expected/$name.rc"  "tests/tmp/$name.rc"; then
        printf '  PASS golden %s\n' "$name"
    else
        printf '  FAIL golden %s\n' "$name"
        fail=1
    fi
done

$CC -std=c23 -Wall -Wextra -Werror -o sumcsv-buggy main.c parse.c sum_buggy.c
brc=0
./sumcsv-buggy < tests/in/06_trailing_comma.txt > tests/tmp/bug.out 2> tests/tmp/bug.err || brc=$?
printf '%s\n' "$brc" > tests/tmp/bug.rc
if cmp -s tests/expected/06_trailing_comma.out tests/tmp/bug.out &&
   cmp -s tests/expected/06_trailing_comma.err tests/tmp/bug.err &&
   cmp -s tests/expected/06_trailing_comma.rc  tests/tmp/bug.rc; then
    printf '  FAIL regression-before: buggy binary unexpectedly passed\n'
    fail=1
else
    printf '  PASS regression-before: buggy caught by 06_trailing_comma (rc=%d, out=%s)\n' \
        "$brc" "$(tr -d '\n\r' < tests/tmp/bug.out)"
fi

if cmp -s tests/expected/06_trailing_comma.rc tests/tmp/06_trailing_comma.rc; then
    printf '  PASS regression-after: fixed passes 06_trailing_comma (rc=%s)\n' \
        "$(tr -d '\n\r' < tests/tmp/06_trailing_comma.rc)"
fi

make -s CC="$CC" clean
rm -rf sumcsv-buggy tests/tmp
exit "$fail"
