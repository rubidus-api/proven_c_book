#!/bin/sh
# 같은 alias.c 를 세 가지로 빌드해 돌린다. 프로그램은 계약 밖이라 결과는 약속되지 않는다 ---
# 이 시연이 보이는 것은 「최적화가 규칙을 *믿고* 움직인다」는 것뿐이다.
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
