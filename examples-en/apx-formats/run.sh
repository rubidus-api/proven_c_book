#!/bin/sh
# 실행 파일 형식 시연 --- 세 가지 형식을 *직접 만들어* 같은 질문을 던진다.
#
# ★ 왜 스크립트인가 --- `read_headers` 는 파일을 인자로 받는다. 그리고 그 파일들이
#   먼저 있어야 한다: PIE 실행 파일, 고정 주소 실행 파일, 그리고 윈도우 실행 파일.
# ★ 윈도우 파일은 *이 자리에서 만든다.* 기계에 있던 남의 프로그램을 가져다 쓰면
#   재현되지 않고, 남의 사정이 책에 새어 든다. mingw 가 없으면 그 줄만 건너뛰고
#   *건너뛰었다고 말한다* --- 조용히 빠지면 독자는 형식이 둘뿐인 줄 안다.
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
