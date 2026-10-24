#!/bin/sh
# Reproduce the stale-artifact accident.
#
#   Widen GREET_MAX from 16 to 32 in the header and touch only greet.c.
#   The rule with no dependencies rebuilds greet.o alone: greet now writes
#   trusting 32 while main.o still reserves the old 16. The buffer overflows.
set -eu
cd "$(dirname "$0")"

for kind in broken fixed; do
    printf '== Makefile.%s\n' "$kind"
    make -s -f "Makefile.$kind" clean >/dev/null
    make -s -f "Makefile.$kind" CFLAGS="-std=c23 -Wall -Wextra -g -fsanitize=address$([ $kind = fixed ] && printf ' -MMD -MP')" >/dev/null 2>&1
    printf '   first          : %s\n' "$(./demo 2>&1 | head -1)"

    sed -i 's/#define GREET_MAX 16/#define GREET_MAX 32/' greet.h
    touch greet.c
    make -s -f "Makefile.$kind" CFLAGS="-std=c23 -Wall -Wextra -g -fsanitize=address$([ $kind = fixed ] && printf ' -MMD -MP')" >/dev/null 2>&1
    out=$(./demo 2>&1 | head -3 || true)
    printf '   after widening : %s\n' "$(printf '%s' "$out" | head -1)"
    printf '%s' "$out" | grep -q 'stack-buffer-overflow' && printf '   → ASan: stack-buffer-overflow\n' || true

    sed -i 's/#define GREET_MAX 32/#define GREET_MAX 16/' greet.h
    make -s -f "Makefile.$kind" clean >/dev/null
done
