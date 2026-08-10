#!/usr/bin/env bash
# 서식 예시(조판 견본)를 *배포할 물건*으로 만든다 (RFC-0027, 저자 지시 2026-08-10).
#
# ★ 왜 릴리스에 딸려 보내는가
#   디자인은 「옳고 그름」이 아니라 「보기」의 문제라 게이트가 판정하지 못한다.
#   그 자리를 대신하는 것이 이 견본이다. 그런데 견본이 build/ 에만 있으면
#   *고친 사람만* 본다. 판마다 함께 내보내면, 어느 판의 디자인이 어땠는지
#   나중에도 그 판의 견본 한 장으로 확인된다.
#
# 나오는 것 (dist/)
#   proven_c_book-<ver>-style-specimen.pdf   두 쪽
#   proven_c_book-<ver>-style-specimen.html  혼자 도는 한 파일 (글꼴 내장)
# 그리고 docs/style-specimen.html --- 웹판과 같은 자리에서 바로 본다.
#
# 쓰는 법
#   1. 값을 고친다 --- PDF 는 book/style.typ, 웹은 styles/book.css. 그 둘뿐이다.
#   2. scripts/style-preview.sh   (0.25초, 눈으로 확인)
#   3. 마음에 들면 그때 전체 빌드 --- 이 스크립트는 release.sh 가 부른다.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
typst=${TYPST:-typst}
command -v "$typst" >/dev/null 2>&1 || \
  typst=<workspace>/usr/toolchains/typst/typst
fonts=${FONT_PATH:-$root/../toolchains/fonts}
ver=$(grep -m1 'Current:' "$root/VERSION.md" | sed 's/.*\*\*\(v[0-9.]*\)\*\*.*/\1/')
mkdir -p "$root/dist" "$root/build"

pdf="$root/dist/proven_c_book-$ver-style-specimen.pdf"
"$typst" compile --root "$root" --font-path "$fonts" --input trial=1 --input "ver=$ver 서식 예시" \
    "$root/book/style-specimen.typ" "$pdf"

raw="$root/build/style-specimen.raw.html"
# ★ mode=html --- 이것이 없으면 장치가 조판 분기를 타 클래스가 붙지 않는다
"$typst" compile --root "$root" --font-path "$fonts" --features html \
    --format html --input mode=html --input lang=ko --input trial=1 --input "ver=$ver 서식 예시" \
    "$root/book/style-specimen.typ" "$raw" 2>/dev/null

# ① 혼자 돌아다닐 판 --- 글꼴을 data: 로 심는다
python3 "$root/scripts/style-preview-html.py" "$raw" \
    "$root/dist/proven_c_book-$ver-style-specimen.html" \
    "$root/styles/book.css" --embed-fonts "$root/docs/fonts"
# ② 웹판 옆에 두는 판 --- 같은 글꼴 파일을 나눠 쓴다(용량이 붙지 않는다)
python3 "$root/scripts/style-preview-html.py" "$raw" \
    "$root/docs/style-specimen.html" \
    "$root/styles/book.css" --font-prefix "fonts/"
# ★ 텍스트 판 견본도 웹에 둔다 --- 릴리스를 내지 않고 보여 주려면 여기여야 한다
#   (저자 지시 2026-08-10: 「견본만 다시 깃허브에 올려서 테스트」).
cp "$pdf" "$root/docs/style-specimen.pdf"
rm -f "$raw"

pages=$(python3 -c "
from pypdf import PdfReader; print(len(PdfReader('$pdf').pages))" 2>/dev/null || echo '?')
echo "make-specimen: $ver · ${pages}쪽 → dist/…-style-specimen.{pdf,html} · docs/style-specimen.html"
