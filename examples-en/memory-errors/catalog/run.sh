#!/bin/sh
# A field guide to memory errors --- one small program per error, built and run with sanitizers on.
#
# ★ These programs are outside the contract *on purpose*. So the book prints not their results but
#   only "what the tool called it". If a tool fails to catch one (the expected name
#   does not appear), this script fails --- the claims in the text are re-checked every time.
# ★ Addresses and process IDs differ per build, so they are dropped. What is kept is the error's name.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
export ASAN_OPTIONS=detect_stack_use_after_return=1:detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=0

# Run one error and print the name the tool gave it. Fail if the expected name is missing.
probe() {   # probe FILE EXPECTED-NAME
    $cc -std=c23 -g -O0 -fsanitize=address,undefined -fno-sanitize-recover=all \
        "$1" -o "$tmp/prog"
    "$tmp/prog" >/dev/null 2>"$tmp/err" || true
    name=$(sed -n -e 's/.*ERROR: AddressSanitizer: \(attempting double-free\).*/ASan: \1/p' \
                  -e 's/.*ERROR: AddressSanitizer: \(attempting free on address which was not malloc()-ed\).*/ASan: \1/p' \
                  -e 's/.*ERROR: AddressSanitizer: \([a-z-]*\)[ :].*/ASan: \1/p' \
                  -e 's/.*ERROR: LeakSanitizer: \(detected memory leaks\).*/LSan: \1/p' \
                  -e "s/.*runtime error: \(load of value [0-9]*\).*/UBSan: \1 into bool/p" \
                  -e 's/.*runtime error: \(load of misaligned address\).*/UBSan: \1/p' \
                  "$tmp/err" | head -1)
    case "$name" in
        *"$2"*) printf '  %-17s %s\n' "$1" "$name" ;;
        *)      printf '  %-17s MISSED (expected %s)\n' "$1" "$2"; exit 1 ;;
    esac
}

echo "== size"
probe stack_overflow.c  stack-buffer-overflow
probe heap_overflow.c   heap-buffer-overflow
echo "== lifetime"
probe use_after_free.c  heap-use-after-free
probe double_free.c     double-free
probe bad_free.c        "not malloc()-ed"
probe leak.c            "detected memory leaks"
probe return_local.c    stack-use-after-return
echo "== initialization"
probe bad_bool.c        "load of value 2"
# ASan and UBSan do not look at uninitialised reads --- show exactly that.
$cc -std=c23 -g -O0 -fsanitize=address,undefined uninit.c -o "$tmp/prog"
if "$tmp/prog" >/dev/null 2>"$tmp/err" && [ ! -s "$tmp/err" ]; then
    printf '  %-17s %s\n' uninit.c "ASan+UBSan: nothing reported"
else
    printf '  %-17s %s\n' uninit.c "UNEXPECTED report"; exit 1
fi
# The tool that does look (MSan) exists only in Clang.
if $cc -std=c23 -g -O0 -fsanitize=memory uninit.c -o "$tmp/prog" 2>/dev/null; then
    "$tmp/prog" >/dev/null 2>"$tmp/err" || true
    name=$(sed -n 's/.*WARNING: MemorySanitizer: \([a-z-]*\).*/MSan: \1/p' "$tmp/err" | head -1)
    printf '  %-17s %s\n' uninit.c "${name:-MSan: nothing reported}"
else
    printf '  %-17s %s\n' uninit.c "MSan: not offered by this compiler"
fi
echo "== type"
probe misaligned.c      "misaligned address"
echo "== overlap"
probe overlap.c         memcpy-param-overlap
