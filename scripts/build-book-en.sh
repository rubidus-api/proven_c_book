#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
typst=${TYPST:-typst}
fonts=${FONT_PATH:-$root/../toolchains/fonts}
# 원본이 바뀐 장이 있으면 여기서 드러난다 (빌드는 계속한다)
python3 "$root/scripts/sync-status.py" || true

mkdir -p "$root/build"
# --input lang=en 이 공용 lib.typ 의 장치 라벨을 영어로 고른다 (사본 없음)
"$typst" compile --no-pdf-tags --input lang=en --root "$root" --font-path "$fonts" \
    "$root/book-en/main.typ" "$root/build/book-en.pdf"
echo "build-book-en: build/book-en.pdf"
