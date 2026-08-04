#!/bin/sh
# 수록 예제 전수 검증 (R15, T001).
# examples/ 아래 모든 .c 를 C23으로 빌드·실행하고, 표준 출력을
# build/examples-out/<장>/<이름>.c.out 으로 캡처한다 (책의 #demo 가 읽는다).
# 하나라도 실패하면 비영 종료한다.
set -u

root=$(cd "$(dirname "$0")/.." && pwd)
cc=${CC:-gcc}
cflags="-std=c23 -Wall -Wextra -Werror"
outdir="$root/build/examples-out"
bindir="$root/build/examples-bin"
fail=0
total=0

find "$root/examples" -name '*.c' | sort | while IFS= read -r src; do :; done

for src in $(find "$root/examples" -name '*.c' | sort); do
    total=$((total + 1))
    rel=${src#"$root/examples/"}
    out="$outdir/$rel.out"
    bin="$bindir/${rel%.c}"
    mkdir -p "$(dirname "$out")" "$(dirname "$bin")"

    if ! $cc $cflags -o "$bin" "$src" 2>"$out.ccerr"; then
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
echo "verify-examples: all examples green"
