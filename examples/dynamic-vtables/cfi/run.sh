#!/bin/sh
# cfi.c 를 보통으로(GCC · Clang), 그리고 Clang CFI 로 빌드해 돌린다.
# CFI 는 Clang 옵션이고 -flto 를 요구한다(Clang 19 는 없으면 빌드를 거부한다).
# 링커는 lld.
set -u
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
w="-std=c23 -Wall -Wextra -Werror -O2"
for cc in gcc clang; do
    command -v "$cc" >/dev/null 2>&1 || { echo "== $cc: not found, skipped"; continue; }
    echo "== $cc -O2"
    # shellcheck disable=SC2086
    $cc $w cfi.c -o "$tmp/plain" && "$tmp/plain"
done
echo "== clang -O2 -flto -fvisibility=hidden -fsanitize=cfi-icall"
# shellcheck disable=SC2086
if clang $w -flto -fvisibility=hidden -fuse-ld=lld -fsanitize=cfi-icall \
        -fno-sanitize-trap=cfi-icall cfi.c -o "$tmp/cfi" 2>/dev/null; then
    "$tmp/cfi" 2>"$tmp/err"
    echo "(exit $?)"
    grep -o 'control flow integrity check for type .* failed' "$tmp/err" || true
else
    echo "this toolchain cannot build CFI (needs clang, lld, LTO), skipped"
fi
