#!/bin/sh
# Build the same buffer overflow three ways and run it.
#   (1) no protection       (2) stack protector (canary)       (3) _FORTIFY_SOURCE
#
# ★ The unprotected build *may end without incident* --- that is the point of this demonstration.
#   But its result is outside the contract and may be anything, so only "ran to the end"
#   or "stopped" is printed. The two protected builds must stop.
# ★ The messages the defences print are glibc's (other C libraries differ).
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
ulimit -c 0 2>/dev/null || true

# Build, run, and print in one line what happened.
try() {     # try LABEL FLAGS...
    label=$1; shift
    $cc -std=c23 -Wall -Wextra "$@" overflow.c -o "$tmp/prog"
    # Keep out of the output the notice the shell prints for a stopped process (it contains the PID).
    rc=0
    sh -c '"$0" >"$1" 2>"$2"' "$tmp/prog" "$tmp/out" "$tmp/err" 2>/dev/null || rc=$?
    if [ "$rc" -eq 0 ]; then
        printf '%-26s ran to the end: %s\n' "$label" "$(cat "$tmp/out")"
    else
        printf '%-26s stopped: %s\n' "$label" "$(head -1 "$tmp/err")"
    fi
    stopped=$rc
}

try "no protection"            -O0 -U_FORTIFY_SOURCE -fno-stack-protector
# If the two protected builds did not stop, the claim in the text is broken --- end in failure.
try "-fstack-protector-strong" -O0 -U_FORTIFY_SOURCE -fstack-protector-strong
[ "$stopped" -ne 0 ] || { echo "stack protector did not stop the overflow"; exit 1; }
try "-D_FORTIFY_SOURCE=3 -O2"  -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -fno-stack-protector
[ "$stopped" -ne 0 ] || { echo "_FORTIFY_SOURCE did not stop the overflow"; exit 1; }
