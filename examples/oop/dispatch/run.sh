#!/bin/sh
# dispatch.c 를 최적화 없이, 그리고 최적화해서 빌드한다. 배수만 --- 기계와 회차마다 다르다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
for opt in -O0 -O2; do
    $cc -std=c23 -Wall -Wextra -Werror $opt dispatch.c -o "$tmp/dispatch"
    echo "== $opt"
    "$tmp/dispatch"
done
