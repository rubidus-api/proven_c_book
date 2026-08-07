#!/bin/sh
# HTML 판 생성 (GitHub Pages 용). 결과: docs/ko/index.html, docs/en/index.html
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
typst=${TYPST:-typst}
fonts=${FONT_PATH:-$root/../toolchains/fonts}

build_one() {
    lang=$1; src=$2
    out="$root/docs/$lang"
    mkdir -p "$out"
    "$typst" compile --root "$root" --font-path "$fonts" \
        --features html --format html \
        --input mode=html --input "lang=$lang" \
        "$src" "$out/book.html" 2>/dev/null
    # 장별로 나누고 스타일을 입힌다
    # 개념도 SVG 를 함께 배포한다 (lib.typ 의 figure-svg 가 figures/ 를 가리킨다)
    mkdir -p "$out/figures"
    cp -f "$root/book/figures/$lang"/*.svg "$out/figures/" 2>/dev/null || true
    python3 "$root/scripts/wrap-html.py" "$lang" "$out/book.html" "$out"
    rm -f "$out/book.html"
}

# Jekyll 처리를 건너뛰게 한다 — 레거시 Pages 빌드가 200개 넘는 파일을
# 훑느라 10분 제한에 걸리는 것을 막는다 (2026-08-06)
: > "$root/docs/.nojekyll"

build_one ko "$root/book/main.typ"
[ -f "$root/book-en/main.typ" ] && build_one en "$root/book-en/main.typ"

# 조판에만 나오고 HTML 에서 사라지는 장치를 즉시 알린다
python3 "$root/scripts/check-editions.py" || true
