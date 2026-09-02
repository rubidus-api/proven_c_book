#!/bin/sh
# 이름 붙은 주소 공간 --- *AVR 교차 컴파일러로* 짓는다.
#
# ★ 이 예제는 x86 의 gcc 로는 빌드되지 않는다. `__flash` 는 AVR 백엔드의 낱말이다.
#   그것이 이 절의 요점이기도 하다 --- 주소 공간이 여럿인 기계라야 있는 낱말이다.
# ★ 도구가 없으면 건너뛰되 *건너뛴다고 말한다.* 조용히 빠지면 독자는 이것이
#   지어 본 적 없는 코드라는 사실을 모른다.
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
