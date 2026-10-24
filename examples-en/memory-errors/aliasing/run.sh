#!/bin/sh
# Build the same alias.c three ways and run it. The program is outside the contract, so no result is promised ---
# all this shows is that "optimisation acts on its *trust* in the rule".
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
for opt in "-O0" "-O2" "-O2 -fno-strict-aliasing"; do
    # shellcheck disable=SC2086
    $cc -std=c23 -Wall -Wextra $opt alias.c -o "$tmp/alias"
    printf '%-26s %s\n' "$opt" "$("$tmp/alias")"
done
