#!/bin/sh
# Named address space --- built with an *AVR cross compiler*.
#
# This example does not build with x86 gcc: `__flash` belongs to the AVR backend.
#   That is the point: the word exists on machines with multiple address spaces.
# If the tool is absent, skip but say so. A silent skip would hide that this is
#   code that was never built.
set -eu
cd "$(dirname "$0")"
ws=$(cd ../../../.. && pwd)
avr="$ws/usr/toolchains/avr-gcc-16.1.0-x64-linux/bin/avr-gcc"

if [ ! -x "$avr" ]; then
    echo "(no AVR toolchain here: this example was not built)"
    exit 0
fi

echo "== compiler =="
"$avr" --version | head -1
echo

echo "== 1. strict ISO mode: -std=c23 =="
if "$avr" -mmcu=atmega328p -O2 -std=c23 -c named_space.c -o /dev/null 2>err.txt; then
    echo "  compiled"
else
    sed -n '1p' err.txt | sed 's/^named_space\.c/  named_space.c/'
    echo "  -> __flash is a GNU extension, so the strict mode does not have the word."
fi
rm -f err.txt
echo

echo "== 2. GNU mode: -std=gnu23 -Wall -Wextra -Wpedantic =="
if "$avr" -mmcu=atmega328p -O2 -std=gnu23 -Wall -Wextra -Wpedantic \
        -c named_space.c -o named_space.o 2>warn.txt; then
    if [ -s warn.txt ]; then
        echo "  compiled, with:"
        sed 's/^/    /' warn.txt
    else
        echo "  compiled, and said nothing --- not even about mixing the two spaces"
        echo "  in 'p = q' (see the source). The qualifier is dropped in silence."
    fi
fi
rm -f warn.txt named_space.o
echo

echo "== 3. the same subscript, two instructions =="
"$avr" -mmcu=atmega328p -O2 -std=gnu23 -S named_space.c -o named_space.s
for f in from_ram from_rom; do
    echo "  $f:"
    awk -v fn="$f" '$0 == fn ":" {p=1; next} p && /^\t\.size/ {p=0} p' named_space.s \
        | grep -v '^/\*\|stack_usage\|^\t\.' | sed 's/^\t/    /'
done
echo "  where each array landed:"
awk '/\.section/ { sec = $2; sub(/,.*/, "", sec) }
     /^(ram|rom):/ { name = $0; sub(/:/, "", name); printf "    %-4s -> %s\n", name, sec }' \
    named_space.s
rm -f named_space.s

echo
echo "  * ld reads data memory; lpm reads program memory. The C text was the same"
echo "    'x[i]' in both functions -- the type chose the instruction."
