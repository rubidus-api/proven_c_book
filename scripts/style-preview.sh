#!/usr/bin/env bash
# 조판 견본을 만든다 (RFC-0027, 저자 지시 2026-08-10).
#
# ★ 무엇을 푸는가
#   디자인 한 줄을 고칠 때마다 787쪽을 다시 찍고, 그 결과를 사람이(그리고 AI 가)
#   읽어 확인해야 했다. 느리고 토큰이 많이 든다. 그런데 *디자인을 고칠 때는 본문을
#   시험할 까닭이 없다.* 그래서 모든 장치를 한 번씩 담은 두 쪽짜리 견본만 찍는다.
#
#     책 전체 빌드  약 25초  +  787쪽에서 확인
#     견본 빌드     1초 남짓 +  두 쪽에서 확인
#
# 쓰는 법
#   1. 값을 고친다 --- PDF 는 book/style.typ, 웹은 styles/book.css. 그 둘뿐이다.
#   2. scripts/style-preview.sh
#   3. build/style-preview.{pdf,html} 을 본다. 괜찮으면 그때 전체 빌드.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
typst=${TYPST:-typst}
fonts=${FONT_PATH:-$root/../toolchains/fonts}
mkdir -p "$root/build"
pdf="$root/build/style-preview.pdf"
html="$root/build/style-preview.html"

start=$(date +%s%N)
"$typst" compile --root "$root" --font-path "$fonts" \
    "$root/book/style-specimen.typ" "$pdf"
ms=$(( ($(date +%s%N) - start) / 1000000 ))

# 웹 판 견본 — 같은 장치를 HTML 로도 본다
raw="$root/build/style-preview.raw.html"
if "$typst" compile --root "$root" --font-path "$fonts" --features html \
       --format html "$root/book/style-specimen.typ" "$raw" 2>/dev/null; then
  python3 "$root/scripts/style-preview-html.py" "$raw" "$html" "$root/styles/book.css"
  rm -f "$raw"
  web="· HTML build/style-preview.html"
else
  web="(HTML 견본은 건너뜀)"
fi

pages=$(python3 -c "
from pypdf import PdfReader; print(len(PdfReader('$pdf').pages))" 2>/dev/null || echo '?')
echo "style-preview: ${pages}쪽 · ${ms}ms → build/style-preview.pdf ${web}"
echo "  값을 고치는 자리는 둘뿐이다 --- book/style.typ · styles/book.css"
