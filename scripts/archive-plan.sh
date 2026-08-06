#!/bin/sh
set -eu

plan="${1:-}"

if [ -z "$plan" ]; then
  printf '%s\n' "usage: scripts/archive-plan.sh docs/plans/active/<plan>.md" >&2
  exit 1
fi

case "$plan" in
  docs/plans/active/*.md) ;;
  *)
    printf '%s\n' "plan must be under docs/plans/active/" >&2
    exit 1
    ;;
esac

test -f "$plan" || {
  printf '%s\n' "missing plan: $plan" >&2
  exit 1
}

mkdir -p docs/plans/archive
target="docs/plans/archive/$(basename "$plan")"

if [ -e "$target" ]; then
  printf '%s\n' "archive target already exists: $target" >&2
  exit 1
fi

mv "$plan" "$target"
printf '%s\n' "$target"
