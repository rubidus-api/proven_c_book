#!/bin/sh
# These appendix examples use -O2 and POSIX; the box in the text states
# that limitation.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
$cc -std=c23 -Wall -Wextra -Werror -O2 -o ./cow cow.c
./cow
rm -f ./cow
