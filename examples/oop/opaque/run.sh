#!/bin/sh
# 라이브러리와 쓰는 쪽을 두 번역 단위로 빌드하고, 쓰는 쪽은 그 타입의 크기조차
# 물을 수 없음을 보인다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
$cc -std=c23 -Wall -Wextra -Werror stack.c main.c -o "$tmp/stack"
"$tmp/stack"
printf '#include "stack.h"\nsize_t n = sizeof(pcb_stack);\n' > "$tmp/peek.c"
if $cc -std=c23 -I. -c "$tmp/peek.c" -o "$tmp/peek.o" 2>/dev/null; then
    echo "sizeof(pcb_stack) outside stack.c: compiled (unexpected)"
else
    echo "sizeof(pcb_stack) outside stack.c: refused by the compiler (incomplete type)"
fi
