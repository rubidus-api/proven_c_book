#!/bin/sh
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
cat > "$path" <<EOF
# Plan: $slug

## Request


## Acceptance Criteria

-

## TDD Plan

- Test id:
- Verification:

## Expected File Changes

-

## Token Budget

- Read:
- Avoid:

## Stop Conditions

- Stop after three repeated failures for the same reason.
- Stop if requirements conflict or destructive ambiguity appears.

## User Questions

-

## Steps

- [ ]
EOF

printf '%s\n' "$path"
