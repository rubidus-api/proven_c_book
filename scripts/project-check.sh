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
  rg -n "$private_pattern" . --glob '!.git/**' && fail "private path or key-like pattern found"
fi

if command -v find >/dev/null 2>&1; then
  for script in $(find scripts -type f -name '*.sh' 2>/dev/null | sort); do
    sh -n "$script"
  done
fi

printf '%s\n' "project-check: ok"
