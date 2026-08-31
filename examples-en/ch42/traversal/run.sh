#!/bin/sh
# 순회 순서의 값을 *두 최적화 수준에서* 잰다.
#
# ★ 왜 둘인가 --- 처음에 검증기의 기본 빌드(-O0)로만 재고 「2.3배」를 얻었다.
#   같은 코드가 -O2 에서는 훨씬 크게 벌어진다. 최적화를 끄면 루프를 도는
#   비용이 커서 *캐시의 차이가 묻힌다.* 한쪽만 적으면 어느 쪽이든 거짓말이
#   된다. 그래서 둘 다 적고, 배수만 낸다(절대 시간은 기계의 것이다).
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
for opt in -O0 -O2; do
    printf '== %s\n' "$opt"
    $cc -std=c23 -Wall -Wextra -Werror $opt -o ./traversal traversal.c
    ./traversal
done
rm -f ./traversal
