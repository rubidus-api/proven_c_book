#!/bin/sh
# 릴리스 묶음 생성: dist/proven_c_book-<version>-{ko,en}.zip 와 -all.zip
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
ver=$(grep -m1 'Current:' "$root/VERSION.md" | sed 's/.*\*\*\(v[0-9.]*\)\*\*.*/\1/')
# ── 릴리스 게이트 (RFC-0008 §1-F) ───────────────────────────────
# 경고를 안고 배포하지 않는다. 서두 정형과 판 동기는 통과해야 한다.
python3 "$root/scripts/check-chapter-openings.py" || {
    echo "release: 장 서두 정형 검사 실패 — 배포를 중단한다" >&2; exit 1; }
python3 "$root/scripts/check-floats.py" || {
    echo "release: 표·그림 번호 검사 실패 — 배포를 중단한다" >&2; exit 1; }
# README·VERSION 의 판 번호는 book/main.typ 에서 뿌린다 — 어긋난 채 배포하지 않는다
python3 "$root/scripts/sync-version.py" --check || {
    echo "release: 판 번호가 어긋난다 — 배포를 중단한다" >&2; exit 1; }
# 판 사이의 장 누락과 HTML 에서 사라진 장치를 잡는다
python3 "$root/scripts/check-editions.py" || {
    echo "release: 판/산출물 누락 검사 실패 — 배포를 중단한다" >&2; exit 1; }
python3 "$root/scripts/check-inline-code.py" >/dev/null || {
    python3 "$root/scripts/check-inline-code.py" >&2
    echo "release: 인라인 코드가 줄을 넘는다 — 배포를 중단한다" >&2; exit 1; }
if ! python3 "$root/scripts/sync-status.py" >/dev/null; then
    python3 "$root/scripts/sync-status.py" >&2
    echo "release: 한국어판과 영어판의 동기가 어긋난다(stale) — 배포를 중단한다" >&2
    exit 1
fi

# 의존 관계도는 원고에서 뽑아내므로 배포 직전에 다시 만든다(손으로 갱신하지 않는다)
python3 "$root/scripts/make-depgraph.py" >/dev/null || {
    echo "release: 의존 관계도 생성 실패 — 배포를 중단한다" >&2; exit 1; }

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
