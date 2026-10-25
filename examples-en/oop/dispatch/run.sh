#!/bin/sh
# Build dispatch.c without and with optimisation; ratios only, they vary by machine and run.
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
