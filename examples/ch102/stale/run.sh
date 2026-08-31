#!/bin/sh
# 낡은 산출물 사고를 재현한다.
#
#   헤더의 GREET_MAX 를 16 → 32 로 넓히고 greet.c 만 손댄다.
#   의존을 적지 않은 규칙은 greet.o 만 다시 만든다 --- greet 는 32 를 믿고 쓰는데
#   main.o 는 옛 16 으로 그릇을 잡은 채 남는다. 곧 *버퍼가 넘친다.*
set -eu
cd "$(dirname "$0")"

for kind in broken fixed; do
    printf '== Makefile.%s\n' "$kind"
    make -s -f "Makefile.$kind" clean >/dev/null
    make -s -f "Makefile.$kind" CFLAGS="-std=c23 -Wall -Wextra -g -fsanitize=address$([ $kind = fixed ] && printf ' -MMD -MP')" >/dev/null 2>&1
    printf '   처음        : %s\n' "$(./demo 2>&1 | head -1)"

    sed -i 's/#define GREET_MAX 16/#define GREET_MAX 32/' greet.h
    touch greet.c
    make -s -f "Makefile.$kind" CFLAGS="-std=c23 -Wall -Wextra -g -fsanitize=address$([ $kind = fixed ] && printf ' -MMD -MP')" >/dev/null 2>&1
    out=$(./demo 2>&1 | head -3 || true)
    printf '   헤더를 넓힌 뒤: %s\n' "$(printf '%s' "$out" | head -1)"
    printf '%s' "$out" | grep -q 'stack-buffer-overflow' && printf '   → ASan: stack-buffer-overflow\n' || true

    sed -i 's/#define GREET_MAX 32/#define GREET_MAX 16/' greet.h
    make -s -f "Makefile.$kind" clean >/dev/null
done
