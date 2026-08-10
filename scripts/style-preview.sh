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
# ★ 「견본만 고치기」 (저자 지시 2026-08-10, RFC-0027 §5)
#   책을 건드리지 않고 값을 시험해 볼 수 있다. 이 스크립트는 `--input trial=1`
#   을 주고, style.typ 의 값들은 그때만 *시험 칸*을 쓴다.
#
#     #let body-leading = _pick(0.78em, 0.94em)   왼쪽=책, 오른쪽=시험
#
#   웹도 같다 --- styles/book-trial.css 는 견본에만 얹힌다.
#   시험 값이 마음에 들면 **왼쪽에 옮겨 적는다.** 그것이 채택이고, 책은 그때
#   딱 한 번 다시 찍는다.
#
# 쓰는 법
#   1. 시험할 값을 고친다 --- PDF 는 book/style.typ 의 _pick 오른쪽,
#      웹은 styles/book-trial.css.
#   2. scripts/style-preview.sh
#   3. build/style-preview.{pdf,html} 을 본다.
#   4. 저자에게 보일 때는 scripts/make-specimen.sh → docs/style-specimen.html
#      *만* 커밋·푸시한다. 릴리스도, 787쪽 재빌드도 하지 않는다.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
typst=${TYPST:-typst}
fonts=${FONT_PATH:-$root/../toolchains/fonts}
mkdir -p "$root/build"
pdf="$root/build/style-preview.pdf"
html="$root/build/style-preview.html"

start=$(date +%s%N)
"$typst" compile --root "$root" --font-path "$fonts" --input trial=1 \
    "$root/book/style-specimen.typ" "$pdf"
ms=$(( ($(date +%s%N) - start) / 1000000 ))

# 웹 판 견본 — 같은 장치를 HTML 로도 본다
raw="$root/build/style-preview.raw.html"
# ★ `--input mode=html` 을 빠뜨리면 장치들이 *조판 분기*를 타고 클래스 없는 맨
#   <div> 로 나온다 --- 웹 견본이 통째로 밋밋해 보였던 까닭이다(저자 지적).
if "$typst" compile --root "$root" --font-path "$fonts" --features html \
       --format html --input mode=html --input lang=ko --input trial=1 \
       "$root/book/style-specimen.typ" "$raw" 2>/dev/null; then
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
