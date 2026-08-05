#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
typst=${TYPST:-typst}
fonts=${FONT_PATH:-$root/../toolchains/fonts}
mkdir -p "$root/build"
"$typst" compile --root "$root" --font-path "$fonts" \
    "$root/book-en/main.typ" "$root/build/book-en.pdf"
echo "build-book-en: build/book-en.pdf"
