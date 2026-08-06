#!/bin/sh
set -eu

missing=0

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf '%s\n' "missing required tool: $1"
    missing=1
  fi
}

want() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf '%s\n' "missing optional tool: $1"
  fi
}

need sh
need git
want rg
want python3

if [ "$missing" -ne 0 ]; then
  printf '%s\n' "Install the missing required tools, or ask the user to approve an alternative local command."
  exit 1
fi

# 글꼴 — 없으면 조판은 성공하되 대체 글꼴로 떨어진다(한국어판이 그렇게
# v0.3.0·v0.4.0 을 내보냈다). 빌드에도 같은 검사가 있지만 여기서 먼저 알린다.
root=$(cd "$(dirname "$0")/.." && pwd)
fonts=${FONT_PATH:-$root/../toolchains/fonts}
for f in noto-cjk-kr/NotoSerifCJKkr-Regular.otf noto-cjk-kr/NotoSansCJKkr-Regular.otf \
         noto-latin/NotoSerif-Regular.ttf noto-latin/NotoSans-Regular.ttf \
         noto-latin/NotoSansMono-Regular.ttf d2coding/D2Coding-Ver1.3.2-20180524.ttf; do
  [ -f "$fonts/$f" ] || printf '%s\n' "missing font: $fonts/$f"
done

printf '%s\n' "check-tools: ok"
