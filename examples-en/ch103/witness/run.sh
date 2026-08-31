#!/bin/sh
# Stand the witnesses up *before* the fix.
#
#   (1) Run against the version with the defect - a witness must cry (red).
#       If none cries, that witness guards nothing.
#   (2) Run against the fixed version - all must be quiet (green).
#
# The order is the point. A check seen only green is a check you do not know bites.
set -eu
cd "$(dirname "$0")"
CC=${CC:-cc}
CFLAGS="-std=c23 -Wall -Wextra"

printf '== (1) the version with the defect (a witness must cry)\n'
$CC $CFLAGS -o witness-buggy find-buggy.c witness.c
if ./witness-buggy; then
    printf '   * no witness cried - this witness is useless\n'
    rc=1
else
    printf '   -> it cried, as intended\n'
    rc=0
fi

printf '\n== (2) the fixed version (all must be quiet)\n'
$CC $CFLAGS -o witness-fixed find.c witness.c
./witness-fixed || rc=1

rm -f witness-buggy witness-fixed
exit $rc
