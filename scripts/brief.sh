#!/bin/sh
# brief.sh — everything a session needs at the start, in one call.
#
# Prints: the CONTEXT.md resume packet, git branch and status, the active
# backlog focus, open T3 plans, and the last commits. Read this instead of
# opening the files one by one.
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
priv="$(dirname "$root")/$(basename "$root")_private"
cd "$root"

locate() {
  if [ -f "$root/$1" ]; then printf '%s' "$root/$1"
  elif [ -f "$priv/$1" ]; then printf '%s' "$priv/$1"
  else printf ''
  fi
}

# section <file> <heading> : print one `## heading` section of a markdown file.
section() {
  awk -v h="$2" '
    $0 == h { on = 1; print; next }
    on && /^## / { exit }
    on { print }
  ' "$1"
}

ctx=$(locate CONTEXT.md)
if [ -n "$ctx" ]; then
  printf '== resume packet (%s)\n' "$(basename "$(dirname "$ctx")")/CONTEXT.md"
  section "$ctx" "## Resume Packet" | sed '/^[[:space:]]*$/d'
  nx=$(section "$ctx" "## Next Step" | sed '/^[[:space:]]*$/d' | sed '1d')
  [ -n "$nx" ] && { printf '== next step\n%s\n' "$nx"; }
else
  printf '== no CONTEXT.md\n'
fi

printf '== git (%s)\n' "$(git rev-parse --abbrev-ref HEAD 2>/dev/null || printf 'no repository')"
git status --short 2>/dev/null | head -30 || true
n=$(git status --short 2>/dev/null | wc -l | tr -d ' ')
[ "$n" -gt 30 ] && printf '  ... %s more\n' "$((n - 30))"
[ "$n" -eq 0 ] && printf '  clean\n'

bl=$(locate BACKLOGS.md)
if [ -n "$bl" ]; then
  printf '== active focus\n'
  section "$bl" "## Active Focus" | sed '1d;/^[[:space:]]*$/d' | head -12
fi

if ls docs/plans/active/*.md >/dev/null 2>&1; then
  printf '== open plans\n'
  for p in docs/plans/active/*.md; do
    case "$p" in */README.md) continue;; esac
    printf '  %s\n' "$p"
  done
fi

printf '== last commits\n'
git log --date=short --format='  %ad %h %s' -n 5 2>/dev/null || true
