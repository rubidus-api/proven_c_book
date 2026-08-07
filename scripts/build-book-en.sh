#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
typst=${TYPST:-typst}
fonts=${FONT_PATH:-$root/../toolchains/fonts}
# 영어판 예제 트리를 전수 빌드·실행해 캡처를 남긴다 (한국어판과 별개)
"$root/scripts/verify-examples.sh" examples-en

# 원본이 바뀐 장이 있으면 여기서 드러난다 (빌드는 계속한다)
python3 "$root/scripts/sync-status.py" || true
# 장 번호 참조가 원본과 어긋나지 않았는지 대조한다 (빌드는 계속한다)
python3 "$root/scripts/check-xrefs.py" || true
# 줄바꿈된 인라인 코드가 우발적 장 제목이 되는 사고를 막는다
python3 "$root/scripts/check-headings.py" || true
# 장 서두 정형: 기댄 것 → 인출 → 목표 → 질문 (RFC-0008 §2)
python3 "$root/scripts/check-chapter-openings.py" || true

mkdir -p "$root/build"
# --input lang=en 이 공용 lib.typ 의 장치 라벨을 영어로 고른다 (사본 없음)
# 글꼴을 못 찾으면 조판은 성공하되 대체 글꼴로 떨어지므로 경고를 오류로 본다
# (한국어판이 그렇게 v0.3.0·v0.4.0 을 내보냈다).
log="$root/build/typst-en.log"
"$typst" compile --input lang=en --root "$root" --font-path "$fonts" \
    "$root/book-en/main.typ" "$root/build/book-en.pdf" >"$log" 2>&1 || {
    cat "$log" >&2; exit 1; }
cat "$log"
if grep -q "unknown font family" "$log"; then
    echo "build-book-en: 글꼴을 찾지 못했다 ($fonts) — 대체 글꼴 PDF 는 버린다" >&2
    grep "unknown font family" "$log" | sort -u >&2
    rm -f "$root/build/book-en.pdf"
    exit 1
fi
echo "build-book-en: build/book-en.pdf"
