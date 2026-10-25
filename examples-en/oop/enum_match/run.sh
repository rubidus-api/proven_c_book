#!/bin/sh
# Build area.c as it is, then again with a third kind of shape added to the enum.
# -Wall includes -Wswitch: a switch over an enum, without default, that misses a value.
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
