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
    python3 "$root/scripts/wrap-html.py" "$lang" "$out/book.html" "$out"
    rm -f "$out/book.html"
}

build_one ko "$root/book/main.typ"
[ -f "$root/book-en/main.typ" ] && build_one en "$root/book-en/main.typ"
