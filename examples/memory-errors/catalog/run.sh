#!/bin/sh
# 메모리 사고 도감 --- 사고마다 작은 프로그램 하나를 새니타이저를 켜고 빌드해 돌린다.
#
# ★ 이 프로그램들은 *일부러* 계약 밖이다. 그래서 책은 소스의 결과가 아니라
#   「도구가 무엇이라 불렀는가」만 싣는다. 도구가 잡지 못하면(예상한 이름이
#   보이지 않으면) 이 스크립트가 실패한다 --- 본문의 주장이 늘 다시 확인된다.
# ★ 주소·프로세스 번호는 빌드마다 달라서 지운다. 남기는 것은 사고의 이름이다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
export ASAN_OPTIONS=detect_stack_use_after_return=1:detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=0

# 사고 하나를 돌려 도구가 붙인 이름을 찍는다. 예상한 이름이 없으면 실패.
probe() {   # probe 파일 예상-이름
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
# 초기화하지 않은 읽기는 ASan·UBSan 이 보지 않는다 --- 그것을 그대로 보인다.
$cc -std=c23 -g -O0 -fsanitize=address,undefined uninit.c -o "$tmp/prog"
if "$tmp/prog" >/dev/null 2>"$tmp/err" && [ ! -s "$tmp/err" ]; then
    printf '  %-17s %s\n' uninit.c "ASan+UBSan: nothing reported"
else
    printf '  %-17s %s\n' uninit.c "UNEXPECTED report"; exit 1
fi
# 그것을 보는 도구(MSan)는 Clang 에만 있다.
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
