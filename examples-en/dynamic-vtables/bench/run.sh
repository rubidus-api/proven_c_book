#!/bin/sh
# Builds ways.c + harness.c with GCC and with Clang at -O2 and runs each once.
# Ratios only --- they differ by machine and run. If taskset is there and the machine
# has more than two CPUs, the run is pinned to one CPU.
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
pin=""
if command -v taskset >/dev/null 2>&1 && [ "$(nproc 2>/dev/null || echo 1)" -gt 2 ]; then
    pin="taskset -c 2"
fi
shape() {   # did the compiler turn m_switch into a jump table (an indirect jump)?
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
