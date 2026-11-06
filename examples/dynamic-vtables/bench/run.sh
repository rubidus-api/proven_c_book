#!/bin/sh
# ways.c + harness.c 를 GCC 와 Clang 으로 -O2 빌드해 한 번씩 돌린다.
# 배수만 --- 기계와 회차마다 다르다. taskset 이 있고 CPU 가 셋 이상이면
# 한 CPU 에 묶어 돌린다.
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
pin=""
if command -v taskset >/dev/null 2>&1 && [ "$(nproc 2>/dev/null || echo 1)" -gt 2 ]; then
    pin="taskset -c 2"
fi
shape() {   # 컴파일러가 m_switch 를 점프 표(간접 점프 하나)로 냈는가?
    command -v objdump >/dev/null 2>&1 || return 0
    objdump -d --no-show-raw-insn "$1" |
        awk '/<m_switch>:/ { p = 1; next } /^$/ { p = 0 } p' > "$tmp/s.s"
    [ -s "$tmp/s.s" ] || return 0
    if grep -Eq 'jmp[a-z]* +\*|thunk|retpoline|[[:space:]]br[[:space:]]' "$tmp/s.s"; then
        echo "switch compiled to: a jump table (one indirect jump)"
    else
        echo "switch compiled to: a chain of compares (no indirect jump)"
    fi
}
for cc in gcc clang; do
    command -v "$cc" >/dev/null 2>&1 || { echo "== $cc: not found, skipped"; continue; }
    $cc -std=c23 -Wall -Wextra -Werror -O2 ways.c harness.c -o "$tmp/bench"
    echo "== $cc -O2"
    $pin "$tmp/bench"
    shape "$tmp/bench"
done
