#!/bin/sh
# stage-pages.sh — 공개할 사이트 한 벌을 build/pages/ 에 모은다.
#
# ★ 왜
#   웹판과 PDF 는 main 에 커밋하지 않고 gh-pages 가지에만 둔다. 그 가지는 릴리스 때마다
#   커밋 하나로 통째 갈아 끼운다(작업공간 문서 기준 DS-G5 · DS-G5a). PDF 는 판 번호 없는
#   이름으로 최신 판 하나만 둔다 --- 주소가 바뀌지 않고, 판 번호는 표지와 웹판 패널에 있다
#   (DS-G5b). 릴리스 자산으로는 PDF 를 더 붙이지 않는다.
#
#     빌드(책 두 권 + 웹판 + 견본) → sh scripts/stage-pages.sh
#       → 발행 도구가 build/pages/ 를 gh-pages 로 낸다(태그를 줄 때만)
#
#   여기서는 모으고 확인만 한다. 아무것도 올리지 않는다.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
out=build/pages
ver=$(grep -m1 'Current:' VERSION.md | sed 's/.*\*\*\(v[0-9.]*\)\*\*.*/\1/')
[ -n "$ver" ] || { echo "stage-pages: VERSION.md 에서 판 번호를 읽지 못했다" >&2; exit 1; }

# 1. 빌드가 다 있는가
for f in docs/ko/index.html docs/en/index.html docs/fonts docs/style-specimen.html docs/style-specimen.pdf \
         build/book.pdf build/book-en.pdf ops/doc-src/site/index.html; do
  [ -e "$f" ] || { echo "stage-pages: $f 가 없다 --- 먼저 빌드한다" >&2; exit 1; }
done

# 2. 이 판의 것인가: 웹판 두 판에 판 번호가 찍혀 있고, PDF 가 VERSION.md 보다 나중에 만들어졌다
for l in ko en; do
  grep -q "$ver" "docs/$l/index.html" || { echo "stage-pages: docs/$l 이 $ver 가 아니다 --- 웹판을 다시 빌드한다" >&2; exit 1; }
done
for p in build/book.pdf build/book-en.pdf; do
  [ "$p" -nt VERSION.md ] || { echo "stage-pages: $p 가 VERSION.md 보다 오래되었다 --- 책을 다시 빌드한다" >&2; exit 1; }
done

# 3. 모은다
rm -rf "$out"
mkdir -p "$out"
cp -R docs/ko docs/en docs/fonts "$out/"
cp docs/style-specimen.html docs/style-specimen.pdf "$out/"
cp ops/doc-src/site/index.html "$out/index.html"
: > "$out/.nojekyll"
cp build/book.pdf "$out/proven_c_book-ko.pdf"
cp build/book-en.pdf "$out/proven_c_book-en.pdf"
find "$out" -name __pycache__ -prune -exec rm -rf {} +

n=$(find "$out" -type f | wc -l | tr -d ' ')
size=$(du -sh --apparent-size "$out" 2>/dev/null || du -sh "$out"); size=${size%%	*}
echo "stage-pages: $ver --- 파일 $n 개, $size → $out/"
echo "  공개(저자 승인 뒤): pub publish proven_c_book --release --tag $ver --execute"
