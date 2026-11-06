#!/bin/sh
# Builds cfi.c the ordinary way (GCC, Clang), then with Clang CFI, and runs each.
# CFI is a Clang option and needs -flto (Clang 19 refuses the build without it);
# the linker is lld.
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
