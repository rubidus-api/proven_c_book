#!/bin/sh
# area.c 를 그대로 빌드하고, 다시 열거형에 셋째 도형을 더해 빌드한다.
# -Wall 에는 -Wswitch 가 들어 있다: default 없는 열거형 switch 가 값을 빠뜨리면 알린다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
$cc -std=c23 -Wall -Wextra -Werror area.c -o "$tmp/area"
echo "two kinds   : $("$tmp/area")"
if $cc -std=c23 -Wall -Wextra -Werror -DADD_TRIANGLE area.c -o "$tmp/area3" 2>"$tmp/err"; then
    echo "three kinds : built without a word (the switch was not checked)"
else
    echo "three kinds : the build stops ---"
    grep -o "enumeration value.*" "$tmp/err" | head -1 | sed 's/^/  /'
fi
