#!/bin/sh
# The executable-format demonstration --- *build* three formats and ask them the same questions.
#
# ★ Why a script --- `read_headers` takes files as arguments, and those files must
#   exist first: a PIE executable, a fixed-address executable, and a Windows executable.
# ★ The Windows file is *built right here.* Borrowing someone else's program from the
#   machine would not reproduce, and other people's circumstances would leak into the book.
#   Without mingw that line is skipped *and says so* --- silently, the reader would think there are two formats.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
win=x86_64-w64-mingw32-gcc

$cc -std=c23 -Wall -Wextra -O0 -o ./rh read_headers.c
$cc -std=c23 -Wall -Wextra -O0 -o ./pie ../apx-elf-segments/elf_segments.c
$cc -std=c23 -Wall -Wextra -O0 -no-pie -o ./fixed ../apx-elf-segments/elf_segments.c

files="./pie ./fixed"
if command -v "$win" >/dev/null 2>&1; then
    printf 'int main(void){return 0;}\n' > ./w32app.c
    "$win" -O0 -o ./w32app.exe ./w32app.c
    files="$files ./w32app.exe"
else
    echo "(no Windows cross-compiler here: the PE example is skipped)"
fi

./rh $files
rm -f ./rh ./pie ./fixed ./w32app.c ./w32app.exe
