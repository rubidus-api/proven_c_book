#!/bin/sh
# RISC-V 베어메탈 --- *진짜 기계 모형에서* 돌린다.
#
# ★ 이 예제는 x86 의 gcc 로는 빌드되지 않는다(`interrupt` 속성의 인자부터 다르다).
#   그것이 이 부록의 요점이기도 하다 --- 베어메탈 C 는 대상 기계를 고른 다음에야
#   말이 된다. 그래서 교차 컴파일러로 짓고 QEMU 의 virt 기계에서 돌린다.
# ★ 도구가 없으면 건너뛰되 *건너뛴다고 말한다.* 조용히 빠지면 독자는 이것이
#   돌아 본 적 없는 코드라는 사실을 모른다.
set -eu
cd "$(dirname "$0")"
ws=$(cd ../../.. && pwd)
rv="$ws/usr/toolchains/riscv-gcc/bin/riscv-none-elf-gcc"
qemu="$ws/usr/toolchains/qemu-riscv/bin/qemu-system-riscv64"

if [ ! -x "$rv" ] || [ ! -x "$qemu" ]; then
    echo "(no RISC-V toolchain here: this example was not built or run)"
    exit 0
fi

"$rv" -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany -O2 -Wall -Wextra \
    -nostdlib -nostartfiles -T rv_virt.ld rv_start.S rv_timer.c -o ./rv.elf
timeout 60 "$qemu" -machine virt -bios none -nographic -kernel ./rv.elf
rm -f ./rv.elf
