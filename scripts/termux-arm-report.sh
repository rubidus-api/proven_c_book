#!/usr/bin/env bash
# Proven C Book --- ARM 실기에서 부록 O·P 실측 예제를 돌려 보고서를 만든다.
#
# 왜 있나: 책의 실측은 x86-64 리눅스 한 대에서 잰 것이다. aarch64 는 에뮬레이터로만
# 돌려 봤고, 에뮬레이터의 시간은 ARM 의 시간이 아니라 싣지 않았다. 안드로이드 폰의
# Termux 는 *진짜 ARM* 이다. 이 스크립트는 그 기계에서 예제를 책과 같은 옵션으로 짓고
# 돌려, 기계 정보와 함께 한 파일로 모은다.
#
# 쓰는 법 (Termux):
#   curl -fsSLo arm-report.sh https://raw.githubusercontent.com/rubidus-api/proven_c_book/main/scripts/termux-arm-report.sh
#   bash arm-report.sh
#
#   - 충전기를 꽂고 화면을 켜 둔 채로 돌린다(절전·발열이 수를 바꾼다).
#   - 8 GB 를 건드리는 promise 예제는 여유 기억이 충분할 때만 돈다. 억지로 돌리려면 FULL=1.
#   - 예제는 기본으로 main 에서 받는다. 특정 판은 REF=v0.93.1 처럼 태그를 준다.
#   - `pkg install util-linux` 로 taskset 이 있으면, 한 스레드 예제는 가장 빠른 코어에 묶어 돌린다
#     (big/little 코어가 섞인 폰에서는 어느 코어에 올라가느냐가 수를 바꾼다). 묶지 않으려면 PIN=none.
#
# 이 스크립트는 기기에 아무것도 설치하지 않는다 --- clang 이 없으면 설치 명령을 알려 주고 멈춘다.
set -u

REF=${REF:-main}
BASE=${BASE:-https://raw.githubusercontent.com/rubidus-api/proven_c_book/$REF}
WORK=${WORK:-$HOME/proven-arm-work}
STAMP=$(date +%Y%m%d-%H%M)
REPORT=${REPORT:-$HOME/proven-arm-report-$STAMP.txt}
SCRIPT_VERSION=2

EXAMPLES="apx-measured/clock_probe apx-measured/cache_ladder apx-measured/stride
apx-measured/tlb_walk apx-measured/branch apx-measured/false_sharing apx-measured/fp_cost
apx-memory/promise apx-memory/firsttouch apx-memory/cow apx-memory/layout"

say() { printf '%s\n' "$*" | tee -a "$REPORT"; }
kv()  { printf '  %-22s %s\n' "$1" "$2" | tee -a "$REPORT"; }

# ── 준비물 ──────────────────────────────────────────────────────────────
if [ -z "${CC:-}" ]; then
    if command -v clang >/dev/null 2>&1; then CC=clang
    elif command -v gcc >/dev/null 2>&1; then CC=gcc
    else
        echo "C 컴파일러가 없습니다. Termux 에서 먼저 설치하세요:"
        echo "  pkg install -y clang"
        exit 1
    fi
fi
command -v curl >/dev/null 2>&1 || { echo "curl 이 없습니다:  pkg install -y curl"; exit 1; }
export CC

: > "$REPORT"
rm -rf "$WORK"; mkdir -p "$WORK"

prop() { command -v getprop >/dev/null 2>&1 && getprop "$1" 2>/dev/null; }
readf() { [ -r "$1" ] && tr -d '\n' < "$1" 2>/dev/null; }
# 칸이 없는지(-), 있는데 못 읽는지(denied), 비어 있는지(empty)를 구별한다
readv() {
    if [ ! -e "$1" ]; then printf -- '-'
    elif [ ! -r "$1" ]; then printf 'denied'
    else v=$(tr -d '\n' < "$1" 2>/dev/null); printf '%s' "${v:-empty}"; fi
}

sha=$(curl -fsSL "https://api.github.com/repos/rubidus-api/proven_c_book/commits/$REF" 2>/dev/null |
      awk -F'"' '/"sha":/{print substr($4,1,12); exit}')
say "===== PROVEN C BOOK ARM REPORT (script v$SCRIPT_VERSION, examples $REF ${sha:-?}) ====="
say "date: $(date '+%Y-%m-%d %H:%M:%S %z')"
say ""
say "== device =="
kv "uname" "$(uname -srm)"
kv "android" "$(prop ro.build.version.release) (sdk $(prop ro.build.version.sdk))"
kv "maker/model" "$(prop ro.product.manufacturer) $(prop ro.product.model)"
kv "soc" "$(prop ro.soc.manufacturer) $(prop ro.soc.model) / $(prop ro.board.platform)"
kv "compiler" "$($CC --version 2>/dev/null | head -1)"
kv "online cpus" "$(nproc 2>/dev/null)"
kv "MemTotal" "$(awk '/^MemTotal/{print $2" kB"}' /proc/meminfo)"
kv "MemAvailable" "$(awk '/^MemAvailable/{print $2" kB"}' /proc/meminfo)"
kv "battery temp" "$(readv /sys/class/power_supply/battery/temp) (0.1 C) at start"
kv "wake lock" "$(command -v termux-wake-lock >/dev/null && echo 'available (run termux-wake-lock first)' || echo 'n/a')"

say ""
say "== cpu cores (implementer / part / max freq / governor) =="
ncpu=$(nproc --all 2>/dev/null || nproc 2>/dev/null || echo 8)
i=0
while [ "$i" -lt "$ncpu" ]; do
    d=/sys/devices/system/cpu/cpu$i
    part=$(awk -v c="$i" '
        /^processor/ {p=$3}
        p==c && /^CPU implementer/ {imp=$4}
        p==c && /^CPU part/ {print imp" "$4; exit}' /proc/cpuinfo 2>/dev/null)
    kv "cpu$i" "${part:-?} | $(readv $d/cpufreq/cpuinfo_max_freq) kHz | $(readv $d/cpufreq/scaling_governor)"
    i=$((i + 1))
done

say ""
say "== caches the kernel describes in sysfs =="
for c in 0 $((ncpu - 1)); do
    dir=/sys/devices/system/cpu/cpu$c/cache
    if [ ! -d "$dir" ]; then say "  cpu$c: no $dir (or not readable)"; continue; fi
    for idx in "$dir"/index*; do
        [ -d "$idx" ] || continue
        kv "cpu$c/$(basename "$idx")" "L$(readv "$idx/level") $(readv "$idx/type") size=$(readv "$idx/size") ways=$(readv "$idx/ways_of_associativity") line=$(readv "$idx/coherency_line_size")"
    done
done

say ""
say "== what sysconf reports (the C library's answer) =="
cat > "$WORK/sysconf.c" <<'EOF'
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#if defined(__aarch64__) && defined(__linux__)
__asm__(".text\n.globl read_ctr_el0\n.type read_ctr_el0, %function\n"
        "read_ctr_el0:\n    mrs x0, ctr_el0\n    ret\n");
unsigned long read_ctr_el0(void);
#endif
int main(void)
{
#ifdef _SC_LEVEL1_DCACHE_SIZE
    printf("  L1d size %ld | assoc %ld | line %ld\n", sysconf(_SC_LEVEL1_DCACHE_SIZE),
           sysconf(_SC_LEVEL1_DCACHE_ASSOC), sysconf(_SC_LEVEL1_DCACHE_LINESIZE));
    printf("  L2 size %ld | L3 size %ld\n", sysconf(_SC_LEVEL2_CACHE_SIZE),
           sysconf(_SC_LEVEL3_CACHE_SIZE));
#else
    printf("  _SC_LEVEL1_DCACHE_SIZE is not defined by this C library\n");
#endif
    printf("  page %ld | cpus %ld\n", sysconf(_SC_PAGESIZE), sysconf(_SC_NPROCESSORS_ONLN));
#if defined(__aarch64__) && defined(__linux__)
    unsigned long ctr = read_ctr_el0();
    printf("  CTR_EL0 0x%lx | smallest D line %ld | smallest I line %ld\n", ctr,
           4L << ((ctr >> 16) & 0xf), 4L << (ctr & 0xf));
#endif
    return 0;
}
EOF
if $CC -std=c23 -O2 -o "$WORK/sysconf" "$WORK/sysconf.c" >"$WORK/sysconf.log" 2>&1; then
    "$WORK/sysconf" | tee -a "$REPORT"
else
    say "  (could not build the sysconf probe)"; sed 's/^/    /' "$WORK/sysconf.log" | tee -a "$REPORT"
fi

avail_kb=$(awk '/^MemAvailable/{print $2}' /proc/meminfo)

# 책이 「GCC 에서만 뜻이 있다」고 적어 둔 예제(교차 검증 건너뜀 목록의 toolchain 줄)
skiplist=$(curl -fsSL "$BASE/docs/example-cross-skip.tsv" 2>/dev/null)

# 가장 빠른 코어(최대 주파수가 가장 높은 것)
fast=$(for f in /sys/devices/system/cpu/cpu[0-9]*/cpufreq/cpuinfo_max_freq; do
           [ -r "$f" ] && printf '%s %s\n' "$(cat "$f")" "$(echo "$f" | sed 's#.*/cpu\([0-9]*\)/cpufreq.*#\1#')"
       done | sort -n | tail -1 | awk '{print $2}')
PIN=${PIN:-auto}
if [ "$PIN" = auto ]; then
    if command -v taskset >/dev/null 2>&1 && [ -n "$fast" ]; then PIN=$fast; else PIN=none; fi
fi
need_kb() {                 # 예제마다 필요한 여유 기억(대략)
    case "$1" in
        apx-memory/promise) echo 9500000 ;;
        apx-memory/cow)     echo 1600000 ;;
        apx-measured/tlb_walk|apx-measured/cache_ladder) echo 1000000 ;;
        *) echo 300000 ;;
    esac
}

say ""
say "== examples (built with the book's own run.sh flags, CC=$CC) =="
if [ "$PIN" = none ]; then
    say "  single-thread examples are NOT pinned to a core$(command -v taskset >/dev/null 2>&1 || echo ' (pkg install util-linux for taskset)')"
else
    say "  single-thread examples pinned to cpu$PIN (taskset); multi-thread ones are not"
fi
for ex in $EXAMPLES; do
    name=${ex##*/}
    dir="$WORK/$ex"; mkdir -p "$dir"
    say ""
    say "---------- $ex ----------"
    if ! curl -fsSL "$BASE/examples/$ex/$name.c" -o "$dir/$name.c" ||
       ! curl -fsSL "$BASE/examples/$ex/run.sh" -o "$dir/run.sh"; then
        say "  (download failed: $BASE/examples/$ex/)"; continue
    fi
    why=$(printf '%s\n' "$skiplist" | awk -F'\t' -v p="$ex/run.sh" '$1==p && $2=="toolchain" {print $3}')
    if [ -n "$why" ] && ! "$CC" --version 2>/dev/null | grep -qi 'gcc (\|free software foundation'; then
        say "  SKIPPED --- GCC-only by design (__attribute__((optimize)) is a GCC switch; see docs/example-cross-skip.tsv), CC is $CC"
        continue
    fi
    need=$(need_kb "$ex")
    if [ "${FULL:-0}" != 1 ] && [ -n "$avail_kb" ] && [ "$avail_kb" -lt "$need" ]; then
        say "  SKIPPED --- needs about $((need / 1024)) MB free, MemAvailable is $((avail_kb / 1024)) MB (FULL=1 to force)"
        continue
    fi
    start=$(date +%s)
    runner=""
    case "$ex" in apx-measured/false_sharing) ;; *) [ "$PIN" != none ] && runner="taskset -c $PIN" ;; esac
    ( cd "$dir" && $runner timeout 900 sh ./run.sh ) >"$dir/out.txt" 2>&1
    rc=$?
    grep -v '^#DATA' "$dir/out.txt" | tee -a "$REPORT"
    say "  [exit $rc | $(( $(date +%s) - start )) s${runner:+ | $runner}]"
done

say ""
kv "battery temp" "$(readv /sys/class/power_supply/battery/temp) (0.1 C) at end"
say "===== END OF REPORT ====="
echo
echo "보고서 파일: $REPORT"
echo "위의 ===== PROVEN C BOOK ARM REPORT 부터 END OF REPORT 까지 복사해 전달하면 됩니다."
if command -v termux-clipboard-set >/dev/null 2>&1; then
    echo "(Termux:API 가 있으면:  termux-clipboard-set < \"$REPORT\"  로 한 번에 복사)"
fi
exit 0
