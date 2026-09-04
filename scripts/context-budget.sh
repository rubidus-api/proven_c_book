#!/bin/sh
# context-budget.sh — is the default context still small?
#
# The default context an agent loads every session is AGENTS.md + CONTEXT.md.
# Nothing else keeps them small, so this does: it prints their sizes and exits 1
# when the pair exceeds the budget. Runs from scripts/project-check.sh and gate.sh.
#
#   scripts/context-budget.sh            check (exit 1 over budget)
#   CONTEXT_BUDGET=24576 scripts/...     a different budget for one run, in bytes
#
# The budget is 16384 bytes unless the project sets CONTEXT_BUDGET in
# scripts/gate.conf; the environment wins over both. A project whose current
# context is larger sets its measured size there as a baseline: the check then
# refuses further growth, and the number is a visible debt to lower, not a
# limit that quietly disappeared.
#
# Operating docs may live in ../<project>_private/ (kit option "private-sibling");
# both locations are searched. Sizes of the trigger-read files are printed too,
# with a warning past 32 KB: those are read by section, and a section is hard to
# find in a file that large.
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
priv="$(dirname "$root")/$(basename "$root")_private"
conf="$root/scripts/gate.conf"
conf_budget=""
test -f "$conf" && conf_budget=$(sed -n 's/^CONTEXT_BUDGET=//p' "$conf" | tail -1 | tr -d '"'"'"' ')
budget="${CONTEXT_BUDGET:-${conf_budget:-16384}}"
case "$budget" in ''|*[!0-9]*) budget=16384 ;; esac
warn_at="${CONTEXT_WARN_AT:-32768}"

locate() {
  if [ -f "$root/$1" ]; then printf '%s' "$root/$1"
  elif [ -f "$priv/$1" ]; then printf '%s' "$priv/$1"
  else printf ''
  fi
}

size() { wc -c < "$1" | tr -d ' '; }

row() {
  # row <label> <bytes> <note>
  printf '  %-28s %7s B  ~%5s tok  %s\n' "$1" "$2" "$(( $2 / 4 ))" "$3"
}

total=0
printf '%s\n' "default context (read every session):"
for f in AGENTS.md CONTEXT.md; do
  p=$(locate "$f")
  if [ -n "$p" ]; then
    n=$(size "$p"); total=$((total + n)); row "$f" "$n" ""
  else
    row "$f" 0 "(missing)"
  fi
done
row "total" "$total" "budget $budget"

printf '%s\n' "read on trigger (by section; warn past $warn_at B):"
for f in SPEC.md REQUIREMENTS.md BACKLOGS.md LESSONS.md docs/tests/test-index.md; do
  p=$(locate "$f")
  [ -n "$p" ] || continue
  n=$(size "$p")
  note=""
  [ "$n" -gt "$warn_at" ] && note="WARN: split into docs/ and keep a section index here"
  row "$f" "$n" "$note"
done

if [ "$total" -gt "$budget" ]; then
  printf '%s\n' "context-budget: OVER BUDGET by $((total - budget)) B. Move history out of CONTEXT.md (git log and CHANGELOG.md already hold it; decisions to DECISIONS.md, hazards to LESSONS.md). A project that genuinely needs more sets CONTEXT_BUDGET in scripts/gate.conf." >&2
  exit 1
fi
printf '%s\n' "context-budget: ok ($total of $budget B)"
