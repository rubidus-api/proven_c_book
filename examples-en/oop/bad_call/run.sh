#!/bin/sh
# Build shapes.c plainly, then with Clang's -fsanitize=function, which checks the type of
# every indirect call. GCC has no such option; the script says so and uses Clang if it is there.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
$cc -std=c23 -Wall -Wextra -Werror shapes.c -o "$tmp/plain"
echo "plain build      : $("$tmp/plain")"
if $cc -std=c23 -fsanitize=function -c shapes.c -o "$tmp/probe.o" 2>/dev/null; then
    san=$cc
elif command -v clang >/dev/null 2>&1; then
    echo "($(basename "$cc") has no -fsanitize=function; using clang for the second build)"
    san=clang
else
    echo "(no compiler here offers -fsanitize=function)"
    exit 0
fi
$san -std=c23 -fsanitize=function -fno-sanitize-recover=all shapes.c -o "$tmp/checked"
if "$tmp/checked" >"$tmp/out" 2>"$tmp/err"; then
    echo "checked build    : $(cat "$tmp/out") (not caught)"
else
    echo "checked build    : stopped"
    sed -n 's/.*runtime error: \(.*\)/  \1/p' "$tmp/err" | sed "s|$tmp/||g"
fi
