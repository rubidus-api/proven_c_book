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

printf '%s\n' "check-tools: ok"
