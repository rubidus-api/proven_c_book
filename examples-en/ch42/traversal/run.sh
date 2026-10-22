#!/bin/sh
# Measure the cost of traversal order *at two optimization levels*.
#
# ★ Why two --- at first we measured only with the verifier's default build (-O0) and got "2.3×".
#   The same code spreads far wider at -O2. With optimization off the cost of running
#   the loop is large and *buries the cache difference.* Report only one and either way
#   it is a lie. So we report both, as ratios only (absolute times belong to the machine).
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
for opt in -O0 -O2; do
    printf '== %s\n' "$opt"
    $cc -std=c23 -Wall -Wextra -Werror $opt -o ./traversal traversal.c
    ./traversal
done
rm -f ./traversal
