#!/bin/sh
# 골든 시험 --- 정답을 파일로 굳혀 두고 기계가 견준다.
#   in/X.txt 를 먹여 나온 것을 expected/X.txt 와 diff 로 맞대어 본다.
#   --accept 를 주면 지금 결과를 정답으로 굳힌다(처음 한 번, 그리고 *의도한* 변경 뒤).
set -eu
cd "$(dirname "$0")"
cc -std=c23 -Wall -Wextra -o wordcount wordcount.c
mkdir -p results
fail=0 n=0
for input in in/*.txt; do
    name=$(basename "$input" .txt)
    ./wordcount < "$input" > "results/$name.txt"
    n=$((n + 1))
    if [ "${1:-}" = "--accept" ]; then
        cp "results/$name.txt" "expected/$name.txt"
        continue
    fi
    if ! diff -u "expected/$name.txt" "results/$name.txt" > "results/$name.diff"; then
        printf '  ✗ %s\n' "$name"
        sed -n '3,6p' "results/$name.diff" | sed 's/^/      /'
        fail=$((fail + 1))
    else
        printf '  ✓ %s\n' "$name"
        rm -f "results/$name.diff"
    fi
done
[ "${1:-}" = "--accept" ] && { printf '정답 %d 개를 굳혔다\n' "$n"; exit 0; }
[ "$fail" -eq 0 ] && printf '골든 %d 개 통과\n' "$n" || { printf '어긋남 %d 개\n' "$fail"; exit 1; }
