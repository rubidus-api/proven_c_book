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
skipped=0

# ── 교차 검증에서만 사는 건너뜀 ────────────────────────────────
# ★ 이 책의 약속은 「기준 컴파일러로 전수 통과」다. 기준 판에서는 아래 목록을
#   아예 읽지 않는다 --- 하나도 건너뛰지 않는다. 다른 컴파일러로 교차 검증할
#   때만 목록이 산다(까닭은 docs/example-cross-skip.tsv 에 하나씩 적혀 있다).
#   건너뛴 것은 *반드시 소리 내어* 알린다. 조용한 건너뜀은 없는 검사와 같다.
refcc=${REF_CC:-gcc}
cross=0
[ "$(basename "$cc")" != "$(basename "$refcc")" ] && cross=1
skiplist="$root/docs/example-cross-skip.tsv"

# 이 예제가 교차 검증에서 건너뛸 자리인가. 까닭을 표준출력으로 돌려준다.
cross_reason() {
    [ "$cross" -eq 1 ] || return 1
    [ -f "$skiplist" ] || return 1
    awk -F'\t' -v want="$1" '
        /^#/ || NF < 3 { next }
        $1 == want { print $2 " --- " $3; found = 1; exit }
        END { exit !found }' "$skiplist"
}

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
    # ★ `run.sh` 가 있는 디렉터리는 그 스크립트가 빌드까지 책임진다. 낱낱의
    #   `.c` 를 홀로 빌드하려 들면 안 된다 --- 103장의 증인 시연은 *결함이 있는
    #   판*과 *고친 판*을 나란히 두므로 함께 이으면 중복 정의가 난다.
    if [ -f "$srcdir/run.sh" ]; then
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
    # ★ `atomic_is_lock_free` 같은 것은 libatomic 에 있다. 헤더만 넣고 잇지
    #   않으면 링크에서만 터진다 --- 컴파일은 통과하므로 눈에 늦게 띈다.
    if grep -q '#include <stdatomic.h>' "$src"; then
        extra="$extra -latomic"
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
        if reason=$(cross_reason "$rel"); then
            echo "skip: $rel — $reason"
            skipped=$((skipped + 1))
            rm -f "$out.ccerr"
            continue
        fi
        echo "FAIL build: $rel"
        sed 's/^/    /' "$out.ccerr"
        fail=1
        continue
    fi
    rm -f "$out.ccerr"
    # 목록에 올라 있는데 통과했다면 까닭이 사라진 것이다 --- 줄을 지워야 한다.
    if reason=$(cross_reason "$rel"); then
        echo "FAIL stale-skip: $rel — 이제 통과한다. docs/example-cross-skip.tsv 에서 지울 것"
        fail=1
        continue
    fi

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
    # ★ 기계가 읽을 줄과 사람이 읽을 줄을 가른다 (부록 O 편입, 2026-09-01).
    #   측정 예제는 `#DATA ...` 줄로 잰 값을 함께 낸다. 그것은 도해 생성기가
    #   읽을 것이지 지면에 실릴 것이 아니다 --- 그래서 따로 빼내고 출력에서 지운다.
    #   덕분에 그림이 *이 빌드에서 잰 값*을 그린다.
    if grep -q '^#DATA ' "$out" 2>/dev/null; then
        grep '^#DATA ' "$out" > "${out%.out}.data"
        sed -i '/^#DATA/d' "$out"
    fi
    # 캡처한 출력에 이 기계의 절대 경로가 섞이면 책에 그대로 인쇄된다
    # (53장 예제가 argv[0] 을 찍는다). 저장소 밖의 자리는 책에 실을 것이 아니다.
    if grep -q "$root" "$out" 2>/dev/null; then
        sed -i "s#$root/build/examples-bin-en#.#g; s#$root/build/examples-bin#.#g; s#$root#.#g" "$out"
    fi
    echo "ok: $rel"
done

# ── 스크립트 예제 ────────────────────────────────────────────────
# ★ 102장(빌드와 시험)의 시연은 C 파일 하나가 아니라 *스크립트*다 --- 낡은
#   산출물 사고를 일으키려면 make 를 두 번 돌려야 하고, 골든 시험은 그 자체가
#   러너이기 때문이다. 그래서 `run.sh` 를 만나면 그것을 돌려 출력을 갈무리한다.
#   규약: 예제 디렉터리의 `run.sh` 는 *스스로 치우고* 0 으로 끝나야 한다.
for runner in $(find "$root/$tree" -name 'run.sh' | sort); do
    total=$((total + 1))
    rel=${runner#"$root/$tree/"}
    out="$outdir/$rel.out"
    mkdir -p "$(dirname "$out")"
    if ! sh "$runner" >"$out" 2>&1; then
        if reason=$(cross_reason "$rel"); then
            echo "skip: $rel — $reason"
            skipped=$((skipped + 1))
            continue
        fi
        echo "FAIL run:   $rel"
        sed 's/^/    /' "$out"
        fail=1
        continue
    fi
    if reason=$(cross_reason "$rel"); then
        echo "FAIL stale-skip: $rel — 이제 통과한다. docs/example-cross-skip.tsv 에서 지울 것"
        fail=1
        continue
    fi
    # ★ 스크립트 예제도 `#DATA` 를 낸다. 위쪽 .c 경로에만 갈라내기를 두었더니
    #   측정 예제를 run.sh 로 옮긴 순간 도해가 *옛 자료*를 계속 읽었다
    #   --- 갈라내는 규칙은 출력이 생기는 *모든* 자리에 있어야 한다.
    if grep -q '^#DATA ' "$out" 2>/dev/null; then
        grep '^#DATA ' "$out" > "${out%.out}.data"
        sed -i '/^#DATA/d' "$out"
    fi
    if grep -q "$root" "$out" 2>/dev/null; then
        sed -i "s#$root#.#g" "$out"
    fi
    echo "ok: $rel"
done

if [ "$fail" -ne 0 ]; then
    echo "verify-examples: FAILED"
    exit 1
fi
# ★ 마지막 줄은 *무엇으로 쟀는지*와 *몇을 건너뛰었는지*를 말해야 한다.
#   「all green」 만 적으면 건너뛴 것이 통과한 것처럼 읽힌다.
if [ "$cross" -eq 1 ]; then
    if [ "$skipped" -gt 0 ]; then
        echo "verify-examples: $tree — $(basename "$cc") 교차 검증 통과 · 건너뜀 ${skipped}건 (까닭은 docs/example-cross-skip.tsv)"
    else
        echo "verify-examples: $tree — $(basename "$cc") 교차 검증 전수 통과 · 건너뜀 없음"
    fi
else
    echo "verify-examples: $tree — all examples green ($(basename "$cc"))"
fi
