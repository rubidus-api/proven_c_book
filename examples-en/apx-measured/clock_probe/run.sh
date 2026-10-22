#!/bin/sh
# The measuring examples are run *at -O2*.
#
# ★ Why a script --- what this appendix talks about (optimization deletes work, vector
#   instructions give a width advantage, conditional moves remove branches) happens *only*
#   *with optimization on*. The verifier's default build is -O0, so left alone a demo meant
#   to show "optimization deletes" would print having deleted nothing. That really happened.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
$cc -std=c23 -Wall -Wextra -Werror -O2 -o ./clock_probe clock_probe.c -lm -pthread
./clock_probe
rm -f ./clock_probe
