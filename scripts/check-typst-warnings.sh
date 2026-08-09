#!/usr/bin/env bash
# Typst 가 낸 경고를 하나도 남기지 않는다 (2026-08-09).
#
# ★ 왜 생겼나 --- 마크다운 습관으로 `**굵게**` 라고 쓴 자리가 360군데 있었다.
# Typst 에서 `*` 는 강조를 여닫는 기호라, `**굵게**` 는 *빈 강조 + 굵지 않은 글자*
# 가 된다. 곧 **저자가 굵게 하려던 곳이 하나도 굵지 않았다.** 조판은 성공하고
# PDF 도 나오므로 열 개의 게이트가 전부 통과했다 --- 아무도 경고를 보지 않았기
# 때문이다. Typst 는 이것을 정확히 "no text within stars" 로 알려 주고 있었다.
#
# 교훈은 낱말 하나가 아니라 이것이다: **도구가 이미 하고 있는 말을 게이트가
# 듣지 않으면, 성공하는 빌드가 틀린 결과를 감춘다.**
#
# 그래서 경고를 0으로 못박는다. 새 경고가 하나라도 생기면 릴리스가 막힌다.
#
# 사용법: scripts/check-typst-warnings.sh
# 종료 상태: 경고가 하나라도 있으면 1
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TYPST="${TYPST:-typst}"
FONTS="${FONTS:-$ROOT/../toolchains/fonts}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

total=0
for ed in book book-en; do
  [ -f "$ROOT/$ed/main.typ" ] || continue
  "$TYPST" compile --root "$ROOT" --font-path "$FONTS" \
      "$ROOT/$ed/main.typ" "$TMP/$ed.pdf" 2>"$TMP/$ed.err" >/dev/null
  n=$(grep -c '^warning:' "$TMP/$ed.err" || true)
  total=$((total + n))
  if [ "$n" -gt 0 ]; then
    echo "  ⚠️  [$ed] Typst 경고 ${n}건"
    grep -A3 '^warning:' "$TMP/$ed.err" | grep '┌─' | sed 's/.*┌─ /      /' | head -12
    grep '^warning:' "$TMP/$ed.err" | sort | uniq -c | sed 's/^/      /'
  fi
done

if [ "$total" -gt 0 ]; then
  echo "     ★ 「no text within stars」 라면 마크다운의 **굵게** 를 쓴 것이다."
  echo "       Typst 는 *굵게* --- 별 하나다. 별 둘은 굵어지지 않는다."
  echo "check-typst-warnings: 경고 ${total}건 — 남기지 않는다"
  exit 1
fi
echo "check-typst-warnings: 조판 경고 없다 (양쪽 판)"
