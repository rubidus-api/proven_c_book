#!/bin/sh
# 책 빌드: 예제 전수 검증(출력 캡처) → typst 컴파일 → build/book.pdf
# typst 위치는 PATH 또는 TYPST 환경변수로 지정한다.
# 한글 글꼴(Noto CJK KR)은 FONT_PATH 의 디렉터리에서 찾는다. 기본값은
# toolchains/fonts — 영어판·HTML 빌드와 같은 규칙이다.
#
# ★ 글꼴을 못 찾으면 조판은 성공하지만 한글이 중국어·일본어 대체 글꼴로
#   떨어진다(WenQuanYi Zen Hei, IPAGothic). v0.3.0·v0.4.0 한국어판이 그렇게
#   나갔다. 그래서 여기서는 typst 의 "unknown font family" 경고를 오류로
#   승격시켜 그런 PDF 가 아예 만들어지지 않게 막는다.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
typst=${TYPST:-typst}
command -v "$typst" >/dev/null 2>&1 || {
    echo "build-book: typst not found (set TYPST or PATH)" >&2
    exit 1
}

"$root/scripts/verify-examples.sh"

# 줄바꿈된 인라인 코드가 우발적 장 제목이 되는 사고를 막는다
python3 "$root/scripts/check-headings.py" || true
# 장 서두 정형: 기댄 것 → 인출 → 목표 → 질문 (RFC-0008 §2)
python3 "$root/scripts/check-chapter-openings.py" || true
# 표·그림 번호는 장치가 붙인다 — 날것의 표/그림과 손으로 적은 번호를 막는다
python3 "$root/scripts/check-floats.py" || true
# 판 번호 단일 출처 점검(맞추지는 않고 알리기만 한다)
python3 "$root/scripts/sync-version.py" --check || true
# 인라인 코드가 원고에서 줄을 넘으면 조판에서 두 동강 난다
python3 "$root/scripts/check-inline-code.py" || true

mkdir -p "$root/build"
fonts=${FONT_PATH:-$root/../toolchains/fonts}
log="$root/build/typst-ko.log"
"$typst" compile --root "$root" --font-path "$fonts" \
    "$root/book/main.typ" "$root/build/book.pdf" >"$log" 2>&1 || {
    cat "$log" >&2; exit 1; }
cat "$log"
if grep -q "unknown font family" "$log"; then
    echo "build-book: 글꼴을 찾지 못했다 ($fonts) — 대체 글꼴로 조판된 PDF 는 버린다" >&2
    grep "unknown font family" "$log" | sort -u >&2
    rm -f "$root/build/book.pdf"
    exit 1
fi
echo "build-book: build/book.pdf"

# 한국어판이 바뀌면 영어판이 어긋난다 — 그 사실을 즉시 알린다
python3 "$root/scripts/sync-status.py" || true
