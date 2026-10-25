#!/bin/sh
# Build the library and its user as two translation units, then show that the user
# cannot even ask for the size of the type.
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
