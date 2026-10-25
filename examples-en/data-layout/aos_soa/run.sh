#!/bin/sh
# Build aos_soa.c without and with optimisation and run each once.
# Times differ by machine and by run; the program prints ratios only.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
first=1
for opt in -O0 -O2; do
    $cc -std=c23 -Wall -Wextra -Werror $opt aos_soa.c -o "$tmp/aos_soa"
    [ "$first" -eq 1 ] && "$tmp/aos_soa" | sed -n '1,2p' && first=0
    echo "== $opt"
    "$tmp/aos_soa" | sed -n '3,7p'
done
echo
echo "(ratios only: they change with the machine, the compiler and the run.)"
