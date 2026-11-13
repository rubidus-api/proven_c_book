#!/bin/sh
# 같은 부트 코드(MBR 과 VBR)를 두 번 짓는다. 한 번은 이 기계의 컴파일러와 출력 백엔드로,
# 한 번은 16비트 실모드 코드(gcc -m16)와 진짜 백엔드로 짓고 QEMU 의 PC 기계에서 BIOS 로 부팅한다.
# 디스크 이미지 셋을 임시 디렉터리에 만든다. 파티션 표를 적는 것은 이 책이 아니라 sfdisk 다:
#   1. MBR 표(sfdisk --label dos): 부팅 표시가 붙은 리눅스 파티션 + 스왑
#   2. 1번과 같은 표인데 파티션 첫머리에 VBR 을 쓰지 않은 것 --- 55 aa 가 없다
#   3. GPT(sfdisk --label gpt): 0번 섹터는 보호 MBR 이 된다
# 세 이미지의 첫 440바이트에 이 MBR 을, 1번의 첫 파티션 첫머리에 VBR 을 쓴다.
# 장치에는 아무것도 쓰지 않는다 --- 쓰는 곳은 임시 디렉터리의 파일뿐이다.
set -eu
cd "$(dirname "$0")"
cc=${CC:-gcc}
ws=$(cd ../../../.. && pwd)
tools="$ws/usr/toolchains/disk-tools/root"
if command -v sfdisk >/dev/null 2>&1; then sfdisk=sfdisk
elif [ -x "$tools/usr/sbin/sfdisk" ]; then
    sfdisk="$tools/usr/sbin/sfdisk"; export LD_LIBRARY_PATH="$tools/usr/lib/x86_64-linux-gnu"
else
    echo "(no sfdisk here: no disk image, so nothing was built or run)"; exit 0
fi
if ! echo 'int x;' | $cc -m16 -c -x c -o /dev/null - 2>/dev/null; then
    echo "(this compiler cannot make 16-bit x86 code: nothing was built or run)"; exit 0
fi

dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
# 16비트 실모드 판. 링크 주소는 MBR 이 0x0600, VBR 이 0x7c00.
cf="-std=c23 -m16 -march=i386 -Os -ffreestanding -fno-pic -fno-asynchronous-unwind-tables
    -fno-stack-protector -mregparm=3 -Wall -Wextra -Werror"
# shellcheck disable=SC2086
(cd "$dir" && for s in start.S vstart.S mbr.c vboot.c vbr.c bios_int.c; do
    $cc $cf -c "$OLDPWD/$s" -o "${s%.*}.o"; done)
ld -m elf_i386 --no-warn-rwx-segments -T mbr.ld -o "$dir/mbr.elf" "$dir/start.o" "$dir/mbr.o" "$dir/bios_int.o"
(cd "$dir" && ld -m elf_i386 --no-warn-rwx-segments -T "$OLDPWD/vbr.ld" -o vbr.elf vstart.o vboot.o bios_int.o vbr.o)
objcopy -O binary "$dir/mbr.elf" "$dir/mbr.bin"
objcopy -O binary -R .bss "$dir/vbr.elf" "$dir/vbr.bin"
n=$(wc -c < "$dir/mbr.bin")
if [ "$n" -gt 440 ]; then echo "MBR code is $n bytes; only 440 fit before the disk signature"; exit 1; fi
echo "MBR code: $n of the 440 bytes before the disk signature"

mk() {   # 이름 · 표 형식 · sfdisk 에 줄 표
    img="$dir/$1.img"
    truncate -s 8M "$img"
    dd if="$dir/mbr.bin" of="$img" conv=notrunc 2>/dev/null
    printf '%b' "$3" | "$sfdisk" -q --label "$2" "$img"
    # sfdisk 가 코드 자리를 건드리지 않았는가(디스크 서명은 0x1b8 부터다)
    cmp -s -n "$n" "$dir/mbr.bin" "$img" || { echo "sfdisk overwrote the boot code"; exit 1; }
}
mk mbr dos 'label-id: 0x50524f56\nstart=2048, size=4096, type=83, bootable\nstart=6144, type=82\n'
dd if="$dir/vbr.bin" of="$dir/mbr.img" bs=512 seek=2048 conv=notrunc 2>/dev/null
mk novbr dos 'label-id: 0x50524f56\nstart=2048, size=4096, type=83, bootable\nstart=6144, type=82\n'
mk gpt gpt 'label-id: 50524F56-0000-4000-8000-000000000001\nstart=2048, size=4096, type=L\n'

echo "== this machine: the commands the boot code sends (trace backend) =="
$cc -std=c23 -Wall -Wextra -Werror -O2 -I. -o "$dir/trace" mbr.c vbr.c bios_trace.c
for d in mbr novbr gpt; do echo "-- $d.img"; "$dir/trace" "$dir/$d.img"; done

echo
echo "== QEMU PC (emulated x86, SeaBIOS): the same code booted by the BIOS =="
q="$ws/usr/toolchains/qemu-x86/root"
qemu="$q/usr/bin/qemu-system-i386"
if [ ! -x "$qemu" ]; then echo "(no QEMU for x86 here: this half was not run)"; exit 0; fi
for d in mbr novbr gpt; do
    echo "-- $d.img"
    rc=0
    LD_LIBRARY_PATH="$q/usr/lib/x86_64-linux-gnu" timeout 30 "$qemu" -L "$q/usr/share/seabios" \
        -L "$q/usr/share/qemu" -display none -serial stdio -monitor none -nic none \
        -drive file="$dir/$d.img",format=raw -device isa-debug-exit,iobase=0xf4 > "$dir/out" || rc=$?
    tr -d '\r' < "$dir/out"
    # isa-debug-exit 는 (값 << 1) | 1 로 끝난다: stop(0) 이면 1, stop(1) 이면 3
    case $rc in 1) echo "(stopped: reached the end)" ;; 3) echo "(stopped: the boot code gave up)" ;;
        *) echo "QEMU ended with $rc"; exit 1 ;; esac
done
