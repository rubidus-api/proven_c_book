#!/bin/sh
# 이 드라이버는 출력 백엔드로만 돈다. QEMU 의 RISC-V 기계들에는 I2C 제어기 모형이 없다.
set -eu
cd "$(dirname "$0")"
hw=../../apx-riscv
cc=${CC:-gcc}

echo "== this machine: the commands the driver sends (trace backend) =="
$cc -std=c23 -Wall -Wextra -Werror -O2 -I. -I$hw -o ./trace main.c i2c.c $hw/hw_trace.c
./trace
rm -f ./trace
echo
echo "(QEMU's RISC-V machines model no I2C controller: there is no second half here)"
