#!/bin/sh
# gate.sh — run the minimum verification tier for what changed.
#
# One gate that runs everything is always too big: a one-line doc edit should not
# run the whole test suite, and a suite nobody waits for is a suite nobody runs.
# The tier is chosen from `git status`, not by the agent; an explicit tier may
# raise the choice, never lower it.
#
#   t0  syntax   shell syntax of scripts/                          (instant)
#   t1  docs     t0 + context budget + whitespace check            (seconds)
#   t2  full     t1 + scripts/project-check.sh + TEST_CMD          (the real suite)
#
# Without Git metadata the gate cannot prove that a change is only docs or scripts,
# so automatic selection deliberately falls back to t2.
#
#   scripts/gate.sh            auto-select and run
#   scripts/gate.sh t2         run at least t2
#   scripts/gate.sh --explain  show the selection and exit
#
# Configuration: scripts/gate.conf (KEY=value, one per line, no quoting needed). Keys:
#   TEST_CMD      the project's test command, run at t2 (e.g. `make test`)
#   DOC_GLOBS     extra path globs that count as documentation (space-separated)
#   SCRIPT_GLOBS  extra path globs that count as scripts
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"

conf="scripts/gate.conf"
# KEY=value reader (not sourced: a command with spaces needs no quoting here).
confget() {
  [ -f "$conf" ] || return 0
  sed -n "s/^$1=//p" "$conf" | tail -1 | sed -e 's/^"\(.*\)"$/\1/' -e "s/^'\(.*\)'$/\1/"
}
TEST_CMD=$(confget TEST_CMD)
DOC_GLOBS=$(confget DOC_GLOBS)
SCRIPT_GLOBS=$(confget SCRIPT_GLOBS)

explain=0; want=""
for a in "$@"; do
  case "$a" in
    --explain) explain=1 ;;
    t0|t1|t2) want=$a ;;
    *) printf '%s\n' "gate: unknown argument: $a" >&2; exit 2 ;;
  esac
done

rank() { case "$1" in t0) echo 0;; t1) echo 1;; t2) echo 2;; esac; }

# ---- what changed -> minimum tier ------------------------------------------
has_git=0
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && has_git=1
changed=$(git status --porcelain 2>/dev/null | awk '{ $1 = ""; sub(/^ +/, ""); print }' || true)
[ -n "$changed" ] || changed=$(git diff --name-only HEAD~1 2>/dev/null || true)

matches_any() {
  # matches_any <path> <globs...>
  p=$1; shift
  for g in "$@"; do
    # shellcheck disable=SC2254
    case "$p" in $g) return 0;; esac
  done
  return 1
}

tier=t0
reason="no changes detected"
if [ "$has_git" -eq 0 ]; then
  tier=t2; reason="no git metadata; change scope cannot be proven"
elif [ -n "$changed" ]; then
  tier=t0; reason="only scripts changed"
  for f in $changed; do
    if matches_any "$f" '*.md' 'docs/*' 'docs/**' '*.txt' $DOC_GLOBS; then
      [ "$(rank "$tier")" -lt 1 ] && { tier=t1; reason="documentation changed"; }
    elif matches_any "$f" 'scripts/*' '*.sh' $SCRIPT_GLOBS; then
      :
    else
      tier=t2; reason="source, tests, or configuration changed ($f)"; break
    fi
  done
fi

if [ -n "$want" ]; then
  if [ "$(rank "$want")" -lt "$(rank "$tier")" ]; then
    printf '%s\n' "gate: $want is below the minimum $tier for these changes; tiers can be raised, not lowered" >&2
    exit 2
  fi
  tier=$want; reason="requested"
fi

printf '%s\n' "gate: $tier ($reason)"
[ "$explain" -eq 1 ] && exit 0

# ---- run ----------------------------------------------------------------------
run() { printf '%s\n' "gate: run: $*"; "$@"; }

# t0: syntax
check_shell_syntax() {
  first=$(sed -n '1p' "$1")
  case "$first" in
    *bash*) command -v bash >/dev/null 2>&1 || { printf '%s\\n' "gate: bash is required for $1" >&2; exit 1; }; bash -n "$1" ;;
    *) sh -n "$1" ;;
  esac
}
for s in $(find scripts -type f -name '*.sh' 2>/dev/null | sort); do check_shell_syntax "$s"; done
printf '%s\n' "gate: t0 ok"
[ "$tier" = t0 ] && exit 0

# t1: docs. The privacy scan is not run here: project-check.sh owns that policy
# (a project may allow a path this scanner flags), and a gate that is stricter
# than the project's own check would refuse work the project accepts. It runs at t2.
[ -x scripts/context-budget.sh ] && run scripts/context-budget.sh
[ "$has_git" -eq 0 ] || git diff --check
printf '%s\n' "gate: t1 ok"
[ "$tier" = t1 ] && exit 0

# t2: full
[ -x scripts/project-check.sh ] && run scripts/project-check.sh
if [ -n "$TEST_CMD" ]; then
  printf '%s\n' "gate: run: $TEST_CMD"
  sh -c "$TEST_CMD"
fi
printf '%s\n' "gate: t2 ok"
