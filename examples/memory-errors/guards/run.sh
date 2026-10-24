#!/bin/sh
# 같은 버퍼 오버플로를 세 가지로 빌드해 돌린다.
#   ① 아무 방어 없이       ② 스택 보호자(카나리)       ③ _FORTIFY_SOURCE
#
# ★ 방어가 없는 빌드는 *아무 일 없이 끝날 수 있다* --- 그것이 이 시연의 요점이다.
#   다만 그 빌드의 결과는 계약 밖이라 무엇이든 될 수 있으므로, 「끝났다」와
#   「멈췄다」 가운데 무엇이었는지만 찍는다. 두 방어 빌드는 반드시 멈춰야 한다.
# ★ 방어가 이름을 붙이는 문구는 glibc 의 것이다(다른 C 라이브러리에서는 다르다).
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
ulimit -c 0 2>/dev/null || true

# 빌드하고 돌려서 무엇이 일어났는지 한 줄로 찍는다.
try() {     # try 이름 플래그...
    label=$1; shift
    $cc -std=c23 -Wall -Wextra "$@" overflow.c -o "$tmp/prog"
    # 멈춘 프로세스에 대해 셸이 찍는 알림(프로세스 번호가 섞인다)을 출력에서 뺀다.
    rc=0
    sh -c '"$0" >"$1" 2>"$2"' "$tmp/prog" "$tmp/out" "$tmp/err" 2>/dev/null || rc=$?
    if [ "$rc" -eq 0 ]; then
        printf '%-26s ran to the end: %s\n' "$label" "$(cat "$tmp/out")"
    else
        printf '%-26s stopped: %s\n' "$label" "$(head -1 "$tmp/err")"
    fi
    stopped=$rc
}

try "no protection"            -O0 -U_FORTIFY_SOURCE -fno-stack-protector
# 두 방어 빌드가 멈추지 않았다면 본문의 주장이 깨진 것이다 --- 실패로 끝낸다.
try "-fstack-protector-strong" -O0 -U_FORTIFY_SOURCE -fstack-protector-strong
[ "$stopped" -ne 0 ] || { echo "stack protector did not stop the overflow"; exit 1; }
try "-D_FORTIFY_SOURCE=3 -O2"  -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -fno-stack-protector
[ "$stopped" -ne 0 ] || { echo "_FORTIFY_SOURCE did not stop the overflow"; exit 1; }
