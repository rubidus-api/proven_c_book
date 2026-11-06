#!/bin/sh
# devirt.c 를 -O2 로 컴파일하고 함수마다 기계어에 무엇이 남았는지 센다.
#   inlined  : 호출이 하나도 없다(넓이 계산이 그 자리에 들어왔다)
#   direct   : 이름 있는 함수를 바로 부른다
#   indirect : 포인터를 거쳐 부른다(x86-64 의 call * · jmp *, AArch64 의 blr · br)
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
