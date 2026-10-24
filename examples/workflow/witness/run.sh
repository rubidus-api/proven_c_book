#!/bin/sh
# 증인을 *고치기 전에* 세운다.
#
#   ① 결함이 있는 판에 대고 돌린다 --- 증인이 울어야 한다(빨강).
#      울지 않으면 그 증인은 아무것도 지키지 않는 것이다.
#   ② 고친 판에 대고 돌린다 --- 조용해야 한다(초록).
#
# 이 순서가 요점이다. 초록만 본 검사는 *무는지 모르는* 검사다.
set -eu
cd "$(dirname "$0")"
CC=${CC:-cc}
CFLAGS="-std=c23 -Wall -Wextra"

printf '== (1) the version with the defect (a witness must cry)\n'
$CC $CFLAGS -o witness-buggy find-buggy.c witness.c
if ./witness-buggy; then
    printf '   * no witness cried --- this witness is useless\n'
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
