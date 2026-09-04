#!/bin/sh
# new-plan.sh — create a T3 plan file (architecture, data format, protocol,
# public API, concurrency, security). Smaller work plans in the reply, not here.
set -eu

slug="${1:-}"

if [ -z "$slug" ]; then
  printf '%s\n' "usage: scripts/new-plan.sh <short-task-id>" >&2
  exit 1
fi

case "$slug" in
  *[!A-Za-z0-9._-]*)
    printf '%s\n' "short-task-id may contain only letters, numbers, dot, underscore, and dash" >&2
    exit 1
    ;;
esac

date_value=$(date +%Y-%m-%d)
path="docs/plans/active/${date_value}-${slug}.md"

if [ -e "$path" ]; then
  printf '%s\n' "$path"
  exit 0
fi

mkdir -p docs/plans/active
cat > "$path" <<EOT
# Plan: $slug

## Goal

<one sentence: what will be true when this is done>

## Files

-

## Verification

- <the exact command that shows it works>

## Risks

-

## Questions For The Owner

-

## Steps

- [ ]
EOT

printf '%s\n' "$path"
