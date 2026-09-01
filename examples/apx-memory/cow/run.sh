#!/bin/sh
# 이 부록의 예제는 -O2 로 돌린다. 그리고 POSIX 를 쓴다 --- 본문의 상자가
# 그 한계를 밝힌다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
$cc -std=c23 -Wall -Wextra -Werror -O2 -o ./cow cow.c
./cow
rm -f ./cow
