#!/bin/sh
# stage-pages.sh — 공개할 사이트 한 벌을 build/pages/ 에 모은다.
#
# ★ 왜
#   웹판은 main 에 커밋하지 않고 gh-pages 가지에만 둔다. 그 가지는 릴리스 때마다
#   커밋 하나로 통째 갈아 끼운다(작업공간 문서 기준 DS-G5 · DS-G5a). PDF 는 여기에 두지
#   않는다 --- 언어마다 PDF 와 HTML판 zip 이 릴리스 자산이다(저자 결정 2026-11-27).
#
#     빌드(웹판 + 견본) → sh scripts/stage-pages.sh
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
         ops/doc-src/site/index.html; do
  [ -e "$f" ] || { echo "stage-pages: $f 가 없다 --- 먼저 빌드한다" >&2; exit 1; }
done

# 2. 이 판의 것인가: 웹판 두 판과 첫 쪽에 이 판 번호가 찍혀 있다
for f in docs/ko/index.html docs/en/index.html ops/doc-src/site/index.html; do
  grep -q "$ver" "$f" || { echo "stage-pages: $f 가 $ver 가 아니다 --- 다시 빌드하거나 sync-version.py 를 돌린다" >&2; exit 1; }
done

# 3. 모은다
rm -rf "$out"
mkdir -p "$out"
cp -R docs/ko docs/en docs/fonts "$out/"
cp docs/style-specimen.html docs/style-specimen.pdf "$out/"
cp ops/doc-src/site/index.html "$out/index.html"
: > "$out/.nojekyll"
find "$out" -name __pycache__ -prune -exec rm -rf {} +

n=$(find "$out" -type f | wc -l | tr -d ' ')
size=$(du -sh --apparent-size "$out" 2>/dev/null || du -sh "$out"); size=${size%%	*}
echo "stage-pages: $ver --- 파일 $n 개, $size → $out/"
echo "  공개(저자 승인 뒤): pub publish proven_c_book --release --tag $ver --execute"
