#!/bin/sh
# shapes.c 를 그냥 빌드하고, 다시 Clang 의 -fsanitize=function 으로 빌드한다. 이 옵션은
# 간접 호출마다 함수의 타입을 확인한다. GCC 에는 없다 --- 그렇게 알리고 Clang 이 있으면 그것을 쓴다.
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
