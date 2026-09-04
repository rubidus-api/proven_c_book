#!/bin/sh
# worklog.sh — the work log, generated from git instead of written by hand.
#
#   scripts/worklog.sh            last 30 commits, grouped by day
#   scripts/worklog.sh 100        last 100
#   scripts/worklog.sh --files    also list the files each commit touched
#
# Kits before 0.9.0 asked agents to append to WORKLOG.md after every task; that
# duplicated `git log` and grew to tens of kilobytes nobody read. If an old
# WORKLOG.md exists it is cold history; do not update it.
set -eu

cd "$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"

n=30
files=0
for a in "$@"; do
  case "$a" in
    --files) files=1 ;;
    [0-9]*) n=$a ;;
    *) printf '%s\n' "usage: scripts/worklog.sh [n] [--files]" >&2; exit 2 ;;
  esac
done

if [ "$files" -eq 1 ]; then
  git log --date=short --format='%n%ad %h %s' --name-only -n "$n"
else
  git log --date=short --format='%ad%x09%h %s' -n "$n" | awk -F'\t' '
    $1 != day { day = $1; print "## " day }
    { print "- " $2 }'
fi
