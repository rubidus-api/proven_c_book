#!/bin/sh
# 측정 예제는 *-O2 로* 돌린다.
#
# ★ 왜 스크립트인가 --- 이 부록이 말하는 것들(최적화가 계산을 지운다, 벡터
#   명령이 폭의 이점을 만든다, 조건 이동이 분기를 없앤다)은 *최적화를 켰을 때만*
#   벌어진다. 검증기의 기본 빌드는 -O0 이라, 그대로 두면 「최적화가 지운다」를
#   보이려는 시연이 아무것도 지우지 못한 채 지면에 실린다. 실제로 그렇게 실렸다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
$cc -std=c23 -Wall -Wextra -Werror -O2 -o ./false_sharing false_sharing.c -lm -pthread
./false_sharing
rm -f ./false_sharing
