#!/bin/sh
# 수록 예제 전수 검증 (R15, T001).
# 예제 트리 아래 모든 .c 를 C23으로 빌드·실행하고, 표준 출력을
# build/<캡처디렉터리>/<장>/<이름>.c.out 으로 캡처한다 (책의 #demo 가 읽는다).
# 하나라도 실패하면 비영 종료한다.
#
# 트리는 판마다 하나다 — 한국어판은 examples/, 영어판은 examples-en/.
# 예제의 주석과 출력 문자열이 판마다 다르므로 캡처도 따로 남긴다.
#   verify-examples.sh            → examples/    → build/examples-out/
#   verify-examples.sh examples-en → examples-en/ → build/examples-out-en/
set -u

root=$(cd "$(dirname "$0")/.." && pwd)
cc=${CC:-gcc}
cflags="-std=c23 -Wall -Wextra -Werror"

# 로케일 예제(63~67장)는 ko_KR·de_DE 같은 로케일이 있으면 그 값을, 없으면
# "없음"을 인쇄한다 — 어느 쪽이든 통과한다. 저장소 밖에 로케일을 만들어 두었다면
# 여기서 잡아 준다. 직접 만들려면:
#     localedef -i ko_KR -f UTF-8 <경로>/ko_KR.UTF-8
[ -z "${LOCPATH:-}" ] && [ -d "$root/../usr/locale" ] && \
    LOCPATH=$(cd "$root/../usr/locale" && pwd) && export LOCPATH
tree=${1:-examples}
case "$tree" in
  examples)    outdir="$root/build/examples-out";    bindir="$root/build/examples-bin" ;;
  examples-en) outdir="$root/build/examples-out-en"; bindir="$root/build/examples-bin-en" ;;
  *) echo "verify-examples: 알 수 없는 예제 트리 '$tree'" >&2; exit 2 ;;
esac
fail=0
total=0

# vendor: proven 라이브러리 (예제가 <proven/...>을 include하면 자동 연동)
vinc="$root/vendor/proven/include"
vsrc="$root/vendor/proven/src/proven"
vobj="$root/build/vendor-obj"
vendor_built=0
vplat="$root/vendor/proven/platform"
build_vendor() {
    [ "$vendor_built" -eq 1 ] && return 0
    mkdir -p "$vobj"
    for c in "$vsrc"/*.c; do
        o="$vobj/$(basename "${c%.c}").o"
        if [ ! -f "$o" ] || [ "$c" -nt "$o" ]; then
            $cc -std=c23 -O1 -I"$vinc" -c "$c" -o "$o" || return 1
        fi
    done
    for c in "$vplat"/*.c; do
        o="$vobj/sys_$(basename "${c%.c}").o"
        if [ ! -f "$o" ] || [ "$c" -nt "$o" ]; then
            $cc -std=c23 -O1 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
                -I"$vinc" -c "$c" -o "$o" || return 1
        fi
    done
    vendor_built=1
}

for src in $(find "$root/$tree" -name '*.c' | sort); do
    total=$((total + 1))
    rel=${src#"$root/$tree/"}
    out="$outdir/$rel.out"
    bin="$bindir/${rel%.c}"
    mkdir -p "$(dirname "$out")" "$(dirname "$bin")"

    # 여러 파일 예제 규약: 같은 디렉터리에 main.c가 있으면 그 디렉터리를
    # 통째로 한 프로그램으로 빌드한다(main.c가 대표, 나머지는 건너뛴다).
    srcdir=$(dirname "$src")
    if [ -f "$srcdir/main.c" ] && [ "$(basename "$src")" != "main.c" ]; then
        continue
    fi
    srcs="$src"
    if [ "$(basename "$src")" = "main.c" ]; then
        srcs=$(find "$srcdir" -maxdepth 1 -name '*.c' | sort | tr '\n' ' ')
    fi

    extra=""
    if grep -q '#include <math.h>' "$src"; then
        extra="-lm"
    fi
    if grep -q '#include <proven' "$src"; then
        if ! build_vendor; then
            echo "FAIL vendor build (needed by $rel)"
            fail=1
            continue
        fi
        extra="-I$vinc $vobj/*.o -lm"
    fi

    if ! $cc $cflags -o "$bin" $srcs $extra 2>"$out.ccerr"; then
        echo "FAIL build: $rel"
        sed 's/^/    /' "$out.ccerr"
        fail=1
        continue
    fi
    rm -f "$out.ccerr"

    stdin_file="${src%.c}.in"
    if [ -f "$stdin_file" ]; then
        run_ok=0; "$bin" <"$stdin_file" >"$out" 2>&1 || run_ok=$?
    else
        run_ok=0; "$bin" >"$out" 2>&1 || run_ok=$?
    fi
    if [ "$run_ok" -ne 0 ]; then
        echo "FAIL run:   $rel (exit $run_ok)"
        fail=1
        continue
    fi
    echo "ok: $rel"
done

if [ "$fail" -ne 0 ]; then
    echo "verify-examples: FAILED"
    exit 1
fi
echo "verify-examples: $tree — all examples green"
