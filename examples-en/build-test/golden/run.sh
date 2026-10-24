#!/bin/sh
# A golden test - freeze the right answer in a file and let the machine compare.
#   Feed in/X.txt, then diff what comes out against expected/X.txt.
#   --accept freezes the current result as the answer (the first time, and after
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
[ "${1:-}" = "--accept" ] && { printf 'froze %d answers\n' "$n"; exit 0; }
[ "$fail" -eq 0 ] && printf '%d golden tests passed\n' "$n" || { printf '%d mismatched\n' "$fail"; exit 1; }
