#!/bin/sh
# Compile load.c to assembly at -O0 and -O2 and count the instructions in each function.
# x86-64-v3 is a newer x86-64 level that includes the MOVBE instruction (load and swap in one).
# Counts are for x86-64 only; elsewhere the script says so and stops.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
if [ "$(uname -m)" != "x86_64" ]; then
    echo "this demo counts x86-64 instructions; this machine is $(uname -m)"
    exit 0
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
for opt in "-O0" "-O2" "-O2 -march=x86-64-v3"; do
    # shellcheck disable=SC2086
    $cc -std=c23 -Wall -Wextra -Werror $opt -S -fno-asynchronous-unwind-tables \
        -fcf-protection=none load.c -o "$tmp/load.s"
    for fn in get_u32be_shift get_u32be_bswap get_u32le_shift; do
        # instructions = indented lines that do not start with a dot, from the label to ret
        body=$(awk -v f="$fn:" '$1 == f { on = 1; next }
                 on && /^[ \t]+[a-z]/ { print $1 }
                 on && /^[ \t]+ret/ { exit }' "$tmp/load.s")
        n=$(printf "%s\n" "$body" | grep -c . || true)
        hint=""
        printf '%s\n' "$body" | grep -q '^bswap' && hint=" (uses bswap)"
        printf '%s\n' "$body" | grep -q '^movbe' && hint=" (uses movbe)"
        printf '%-20s %-16s %2d instructions%s\n' "$opt" "$fn" "$n" "$hint"
    done
done
