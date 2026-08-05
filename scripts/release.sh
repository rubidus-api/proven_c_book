#!/bin/sh
# 릴리스 묶음 생성: dist/proven_c_book-<version>-{ko,en}.zip 와 -all.zip
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
ver=$(grep -m1 'Current:' "$root/VERSION.md" | sed 's/.*\*\*\(v[0-9.]*\)\*\*.*/\1/')
stage="$root/build/release"
rm -rf "$stage"; mkdir -p "$stage/ko" "$stage/en"

# 한국어: PDF + HTML
cp "$root/dist/proven_c_book-$ver-ko.pdf" "$stage/ko/proven_c_book-$ver-ko.pdf"
cp -r "$root/docs/ko" "$stage/ko/html"
# 영어: PDF + HTML (있을 때만)
[ -f "$root/build/book-en.pdf" ]  && cp "$root/build/book-en.pdf" "$stage/en/proven_c_book-$ver-en.pdf"
[ -d "$root/docs/en" ] && cp -r "$root/docs/en" "$stage/en/html"
for d in ko en; do
    cp "$root/LICENSE" "$root/LICENSE-CODE" "$root/LICENSE-NOTICE.md" "$stage/$d/"
done

python3 "$root/scripts/make-zip.py" "$stage" "$root/dist" "$ver"
rm -rf "$stage"
ls -la "$root"/dist/*.zip
echo "release: $ver"
