#!/bin/sh
# Compiles devirt.c at -O2 and counts, per function, what the machine code kept:
#   inlined  : no call at all (the area computation was put in its place)
#   direct   : a named function is called directly
#   indirect : a call through a pointer (x86-64 call * / jmp *, AArch64 blr / br)
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
command -v objdump >/dev/null 2>&1 || { echo "objdump not found, skipped"; exit 0; }
for cc in gcc clang; do
    command -v "$cc" >/dev/null 2>&1 || { echo "== $cc: not found, skipped"; continue; }
    $cc -std=c23 -Wall -Wextra -Werror -O2 -c devirt.c -o "$tmp/d.o"
    echo "== $cc -O2"
    for f in known_object never_written visible_outside built_at_run_time from_caller; do
        objdump -d --no-show-raw-insn "$tmp/d.o" |
            awk -v f="<$f>:" '$2 == f { p = 1; next } /^$/ { p = 0 } p' > "$tmp/f.s"
        ind='(call|jmp)[a-z]* +\*|[[:space:]](blr|br)[[:space:]]'
        dir='[[:space:]](call|bl|b|jmp)[a-z]*[[:space:]]+[0-9a-f]+ <'
        if grep -Eq "$ind" "$tmp/f.s"; then k=indirect
        elif grep -Eq "$dir" "$tmp/f.s" && ! grep -Eq "<$f\\+" "$tmp/f.s"; then k=direct
        else k=inlined
        fi
        printf '  %-18s %s\n' "$f" "$k"
    done
done
