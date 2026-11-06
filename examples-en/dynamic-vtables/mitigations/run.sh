#!/bin/sh
# The same benchmark (../bench) under two mitigations, each beside the build it
# should be compared with:
#   retpoline  GCC -mindirect-branch=thunk, Clang -mretpoline
#              (against plain -O2, which is ../bench)
#   CFI        Clang -flto -fvisibility=hidden -fsanitize=cfi-icall
#              (against the same LTO build without CFI, printed first)
# All four are compiler options, not C. A build the toolchain refuses is reported,
# not run.
set -u
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
pin=""
if command -v taskset >/dev/null 2>&1 && [ "$(nproc 2>/dev/null || echo 1)" -gt 2 ]; then
    pin="taskset -c 2"
fi
src="../bench/ways.c ../bench/harness.c"
w="-std=c23 -Wall -Wextra -Werror -O2"
lto="-flto -fvisibility=hidden -fuse-ld=lld"
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
run() {   # label, compiler, options...
    label=$1; cc=$2; shift 2
    echo "== $label"
    # shellcheck disable=SC2086
    if command -v "$cc" >/dev/null 2>&1 && $cc $w "$@" $src -o "$tmp/b" 2>/dev/null; then
        $pin "$tmp/b" short
        case $label in *retpoline*) shape "$tmp/b" ;; esac
    else
        echo "this toolchain cannot build it, skipped"
    fi
}
run "gcc -O2 -mindirect-branch=thunk (retpoline)" gcc -mindirect-branch=thunk
run "clang -O2 -mretpoline" clang -mretpoline
# shellcheck disable=SC2086
run "clang -O2 -flto (the control for CFI)" clang $lto
# shellcheck disable=SC2086
run "clang -O2 -flto -fsanitize=cfi-icall" clang $lto -fsanitize=cfi-icall
