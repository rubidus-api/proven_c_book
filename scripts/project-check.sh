#!/bin/sh
set -eu

fail() {
  printf '%s\n' "project-check: $*" >&2
  exit 1
}

if [ -x scripts/check-tools.sh ]; then
  scripts/check-tools.sh
fi

git status --short >/dev/null 2>&1 || fail "not a git repository or git is unavailable"
git diff --check

if command -v rg >/dev/null 2>&1; then
  private_pattern='(/ho''me/|/Us''ers/|/m''nt/|ssh -''i|BEGIN[[:space:]][A-Z0-9[:space:]]*PRI''VATE[[:space:]]KEY)'
  # data: URI 로 박아 넣은 글꼴(base64)은 그 알파벳에 /ho''me/ 같은 조각이 우연히
  # 나타난다 --- 자리 이름이 아니라 압축된 이진이므로 오탐이다.
  hits=$(rg -n "$private_pattern" . --glob '!.git/**' | grep -v ';base64,' || true)
  [ -z "$hits" ] || { printf '%s\n' "$hits"; fail "private path or key-like pattern found"; }
fi

if command -v find >/dev/null 2>&1; then
  for script in $(find scripts -type f -name '*.sh' 2>/dev/null | sort); do
    first=$(sed -n '1p' "$script")
    case "$first" in
      *bash*) command -v bash >/dev/null 2>&1 || fail "bash is required for $script"; bash -n "$script" ;;
      *) sh -n "$script" ;;
    esac
  done
fi

# 원고 규칙 검사 (RFC-0006 §3.3, RFC-0007 S8)
if [ -f scripts/check-headings.py ]; then
  python3 scripts/check-headings.py || fail "chapter heading rule violated"
fi
if [ -f scripts/check-chapter-openings.py ]; then
  python3 scripts/check-chapter-openings.py || fail "chapter opening rule violated"
fi

# 문체의 잔재 --- 금지 표현이 있거나 기준선보다 늘었으면 잡는다 (RFC-0038)
if [ -f scripts/check-style.py ]; then
  python3 scripts/check-style.py --check || fail "문체 잔재가 늘었다"
fi

# 어려운 낱말이 처음 나올 때 설명이 붙어 있는가 (RFC-0039)
if [ -f scripts/check-acronyms.py ]; then
  python3 scripts/check-acronyms.py --check || fail "장의 첫 자리에서 풀리지 않은 약어가 있다"
fi
if [ -f scripts/check-snippets.py ]; then
  python3 scripts/check-snippets.py --check || fail "지면의 코드 조각이 기준선과 다르다"
fi
if [ -f scripts/check-jargon.py ]; then
  python3 scripts/check-jargon.py --check || fail "설명 없이 등장하는 낱말이 있다"
fi

# 공개 저장소에 이 기계의 절대 경로·인증서가 새지 않았는가
if [ -f scripts/check-privacy.py ]; then
  python3 scripts/check-privacy.py || fail "local paths or credentials in the repository"
fi

printf '%s\n' "project-check: ok"
