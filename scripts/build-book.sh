#!/bin/sh
# 책 빌드: 예제 전수 검증(출력 캡처) → typst 컴파일 → build/book.pdf
# typst 위치는 PATH 또는 TYPST 환경변수로 지정한다.
# 한글 글꼴(Noto CJK KR)은 FONT_PATH 환경변수의 디렉터리에서 찾는다.
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

mkdir -p "$root/build"
fontargs=""
if [ -n "${FONT_PATH:-}" ]; then
    fontargs="--font-path $FONT_PATH"
fi
"$typst" compile --no-pdf-tags --root "$root" $fontargs "$root/book/main.typ" "$root/build/book.pdf"
echo "build-book: build/book.pdf"

# 한국어판이 바뀌면 영어판이 어긋난다 — 그 사실을 즉시 알린다
python3 "$root/scripts/sync-status.py" || true
