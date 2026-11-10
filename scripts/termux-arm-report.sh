#!/usr/bin/env bash
# Proven C Book --- ARM 실기에서 부록 M·N 실측 예제를 돌려 보고서를 만든다.
#
# 왜 있나: 책의 실측은 x86-64 리눅스 한 대에서 잰 것이다. aarch64 는 에뮬레이터로만
# 돌려 봤고, 에뮬레이터의 시간은 ARM 의 시간이 아니라 싣지 않았다. 안드로이드 폰의
# Termux 는 *진짜 ARM* 이다. 이 스크립트는 그 기계에서 예제를 책과 같은 옵션으로 짓고
# 돌려, 기계 정보와 함께 한 파일로 모은다.
#
# 쓰는 법 (Termux) --- 두 줄이면 된다:
#   curl -fsSLo arm-report.sh https://raw.githubusercontent.com/rubidus-api/proven_c_book/main/scripts/termux-arm-report.sh
#   bash arm-report.sh
#   → 끝나면 스크립트 *바로 옆*에 proven-arm-report-날짜.txt 가 생긴다. 그 파일 내용을 그대로 붙여 주면 된다.
#
#   - clang · curl · taskset(util-linux)이 없으면 pkg 로 설치한다(NOINSTALL=1 이면 설치하지 않고 멈춘다).
#   - 돌리는 동안 termux-wake-lock 이 있으면 걸어 둔다(화면이 꺼져 느려지지 않게).
#
#   - 충전기를 꽂고 화면을 켜 둔 채로 돌린다(절전·발열이 수를 바꾼다).
#   - 8 GB 를 건드리는 promise 예제는 여유 기억이 충분할 때만 돈다. 억지로 돌리려면 FULL=1.
#   - 예제는 기본으로 main 에서 받는다. 특정 판은 REF=v0.93.1 처럼 태그를 준다.
#   - `pkg install util-linux` 로 taskset 이 있으면, 한 스레드 예제는 가장 빠른 코어와 *같은 종류의
#     코어 묶음*에 묶어 돌린다(big/little 이 섞인 폰에서는 어느 코어에 올라가느냐가 수를 바꾼다).
#     한 코어가 아니라 묶음인 까닭: 안드로이드는 쉬는 코어를 수시로 끈다 --- 꺼진 코어 하나에 묶으면
#     taskset 이 Invalid argument 로 실패한다(실제로 그랬다). 묶지 않으려면 PIN=none.
#
set -u

REF=${REF:-main}
BASE=${BASE:-https://raw.githubusercontent.com/rubidus-api/proven_c_book/$REF}
WORK=${WORK:-$HOME/proven-arm-work}
STAMP=$(date +%Y%m%d-%H%M)
# 보고서는 스크립트 옆에 둔다(curl | bash 로 돌리면 지금 폴더에).
case "$0" in
    */*) HERE=$(cd "$(dirname "$0")" 2>/dev/null && pwd) ;;
    *)   HERE=$(pwd) ;;
esac
REPORT=${REPORT:-${HERE:-$HOME}/proven-arm-report-$STAMP.txt}
SCRIPT_VERSION=4

EXAMPLES=${EXAMPLES:-"apx-measured/clock_probe apx-measured/cache_ladder apx-measured/stride
apx-measured/tlb_walk apx-measured/branch apx-measured/false_sharing apx-measured/fp_cost
apx-memory/promise apx-memory/firsttouch apx-memory/cow apx-memory/layout"}

say() { printf '%s\n' "$*" | tee -a "$REPORT"; }
kv()  { printf '  %-22s %s\n' "$1" "$2" | tee -a "$REPORT"; }

# ── 준비물 ──────────────────────────────────────────────────────────────
need_pkg() {                 # need_pkg <명령> <패키지>
    command -v "$1" >/dev/null 2>&1 && return 0
    if [ "${NOINSTALL:-0}" = 1 ] || ! command -v pkg >/dev/null 2>&1; then return 1; fi
    echo "[$1 이 없어 설치합니다: pkg install -y $2]"
    pkg install -y "$2" >/dev/null 2>&1 || yes | pkg install "$2" >/dev/null 2>&1
    command -v "$1" >/dev/null 2>&1
}
need_pkg curl curl
[ -z "${CC:-}" ] && { need_pkg clang clang || true; }
[ "${PIN:-auto}" = auto ] && { need_pkg taskset util-linux || true; }
if command -v termux-wake-lock >/dev/null 2>&1; then
    termux-wake-lock 2>/dev/null && trap 'termux-wake-unlock 2>/dev/null' EXIT
fi
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
kv "online cpus" "$(readv /sys/devices/system/cpu/online) (at start; Android switches idle cores off)"
kv "THP enabled" "$(readv /sys/kernel/mm/transparent_hugepage/enabled)"
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
    eval "part_$i=\"\${part:-?}\""
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

# 가장 빠른 코어(최대 주파수가 가장 높은 것)와, 그와 같은 종류(CPU part)의 코어 묶음
fast=$(for f in /sys/devices/system/cpu/cpu[0-9]*/cpufreq/cpuinfo_max_freq; do
           [ -r "$f" ] && printf '%s %s\n' "$(cat "$f")" "$(echo "$f" | sed 's#.*/cpu\([0-9]*\)/cpufreq.*#\1#')"
       done | sort -n | tail -1 | awk '{print $2}')
cluster=""
if [ -n "$fast" ]; then
    eval "fpart=\"\${part_$fast:-?}\""
    i=0
    while [ "$i" -lt "$ncpu" ]; do
        eval "pp=\"\${part_$i:-?}\""
        [ "$pp" = "$fpart" ] && cluster="${cluster:+$cluster,}$i"
        i=$((i + 1))
    done
fi
# 가장 느린 코어(효율 코어)와 그 묶음 --- 백로그의 「효율 코어 쪽 곡선」을 위한 것
slow=$(for f in /sys/devices/system/cpu/cpu[0-9]*/cpufreq/cpuinfo_max_freq; do
           [ -r "$f" ] && printf '%s %s\n' "$(cat "$f")" "$(echo "$f" | sed 's#.*/cpu\([0-9]*\)/cpufreq.*#\1#')"
       done | sort -n | head -1 | awk '{print $2}')
slowcluster=""
if [ -n "$slow" ]; then
    eval "spart=\"\${part_$slow:-?}\""
    i=0
    while [ "$i" -lt "$ncpu" ]; do
        eval "pp=\"\${part_$i:-?}\""
        [ "$pp" = "$spart" ] && slowcluster="${slowcluster:+$slowcluster,}$i"
        i=$((i + 1))
    done
fi
[ "$slowcluster" = "$cluster" ] && slowcluster=""
PIN=${PIN:-auto}
if [ "$PIN" = auto ]; then
    if command -v taskset >/dev/null 2>&1 && [ -n "$cluster" ]; then PIN=$cluster; else PIN=none; fi
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
    say "  single-thread examples pinned to cpus $PIN, the fastest core's kind (taskset); multi-thread ones are not"
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
    if [ -n "$runner" ] && grep -q "^taskset:" "$dir/out.txt"; then
        say "  (pinning failed: $(grep -m1 '^taskset:' "$dir/out.txt") --- running unpinned)"
        runner="unpinned after taskset failed"
        ( cd "$dir" && timeout 900 sh ./run.sh ) >"$dir/out.txt" 2>&1
        rc=$?
    fi
    # 붙여 넣는 길에서 깨지지 않게 흔한 비 ASCII 기호를 바꿔 싣는다
    grep -v '^#DATA' "$dir/out.txt" | sed 's/·/|/g; s/…/.../g; s/—/---/g' | tee -a "$REPORT"
    say "  [exit $rc | $(( $(date +%s) - start )) s${runner:+ | $runner}]"
done


# ── 백로그의 추가 측정 (script v4) ───────────────────────────────────────
# ① 거짓 공유를 코어 묶음에 고정해서: 묶지 않았을 때 손해가 안 보인 까닭이 「두 스레드가
#   서로 다른 묶음(다른 캐시)에 올라갔기 때문」인지 가른다. 두 스레드가 같은 묶음에 있어야 하므로
#   코어가 둘 이상인 묶음만 쓴다.
# ② 효율 코어 쪽 곡선: cache_ladder 를 가장 느린 코어 묶음에 묶어 다시 돈다.
pinned_run() {               # pinned_run <예제> <cpus> <이름표>
    ex=$1; cpus=$2; label=$3; dir="$WORK/$ex"
    say ""
    say "---------- $ex pinned to $label (cpus $cpus) ----------"
    if [ ! -f "$dir/run.sh" ]; then say "  (not downloaded --- skipped)"; return; fi
    if ! command -v taskset >/dev/null 2>&1; then say "  (no taskset --- pkg install util-linux)"; return; fi
    need=$(need_kb "$ex")
    if [ "${FULL:-0}" != 1 ] && [ -n "$avail_kb" ] && [ "$avail_kb" -lt "$need" ]; then
        say "  SKIPPED --- needs about $((need / 1024)) MB free"; return
    fi
    start=$(date +%s)
    ( cd "$dir" && taskset -c "$cpus" timeout 900 sh ./run.sh ) >"$dir/out-$label.txt" 2>&1
    rc=$?
    if grep -q "^taskset:" "$dir/out-$label.txt"; then
        say "  (pinning failed: $(grep -m1 '^taskset:' "$dir/out-$label.txt") --- Android may have switched those cores off)"
        return
    fi
    grep -v '^#DATA' "$dir/out-$label.txt" | sed 's/·/|/g; s/…/.../g; s/—/---/g' | tee -a "$REPORT"
    say "  [exit $rc | $(( $(date +%s) - start )) s | taskset -c $cpus]"
}
ncl() { printf '%s' "$1" | awk -F, '{print NF}'; }
say ""
say "== extra runs: false sharing pinned, and the efficiency cores =="
kv "fast cluster" "${cluster:-?} (cpu${fast:-?}, part ${fpart:-?})"
kv "slow cluster" "${slowcluster:-none found} ${slow:+(cpu$slow, part ${spart:-?})}"
if [ -n "$cluster" ] && [ "$(ncl "$cluster")" -ge 2 ]; then
    pinned_run apx-measured/false_sharing "$cluster" fast-cluster
else
    say "  (fast cluster has fewer than two cores --- false sharing cannot be pinned there)"
fi
if [ -n "$slowcluster" ] && [ "$(ncl "$slowcluster")" -ge 2 ]; then
    pinned_run apx-measured/false_sharing "$slowcluster" slow-cluster
fi
if [ -n "$slowcluster" ]; then
    pinned_run apx-measured/cache_ladder "$slowcluster" slow-cluster
else
    say "  (no separate slow cluster --- the efficiency-core curve is not taken)"
fi

say ""
say "== diagnosis: what /proc/self/smaps says after filling 64 MiB (cow reported 0 MB here) =="
cat > "$WORK/smapsdiag.c" <<'EOF2'
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void dump(const char *path, const char *keys[])
{
    FILE *f = fopen(path, "r");
    char line[256];
    if (!f) { printf("  %s: cannot open\n", path); return; }
    long sum[8] = {0}; int seen[8] = {0}; int lines = 0;
    while (fgets(line, sizeof line, f)) {
        lines++;
        for (int k = 0; keys[k]; k++) {
            size_t n = strlen(keys[k]); long v;
            if (strncmp(line, keys[k], n) == 0 && sscanf(line + n, " %ld", &v) == 1) { sum[k] += v; seen[k]++; }
        }
    }
    fclose(f);
    printf("  %s: %d lines\n", path, lines);
    for (int k = 0; keys[k]; k++)
        printf("    %-16s %9ld kB (%d entries)\n", keys[k], sum[k], seen[k]);
}
int main(void)
{
    const size_t N = 64u << 20;
    char *p = malloc(N);
    if (!p) { puts("  malloc failed"); return 0; }
    memset(p, 7, N);
    long check = 0;                 /* 읽어서 찍는다 --- 안 그러면 -O2 가 memset 을 통째로 지운다 */
    for (size_t i = 0; i < N; i += 4096) check += p[i];
    printf("  filled %zu MiB (check %ld)\n", N >> 20, check);
    const char *roll[] = {"Rss:", "Pss:", "Shared_Dirty:", "Private_Dirty:", "Anonymous:", "Swap:", "AnonHugePages:", 0};
    dump("/proc/self/smaps_rollup", roll);
    dump("/proc/self/smaps", roll);
    const char *st[] = {"VmRSS:", "RssAnon:", "VmSwap:", 0};
    dump("/proc/self/status", st);
    free(p);
    return 0;
}
EOF2
if $CC -std=c23 -O2 -o "$WORK/smapsdiag" "$WORK/smapsdiag.c" >"$WORK/smapsdiag.log" 2>&1; then
    "$WORK/smapsdiag" | tee -a "$REPORT"
else
    say "  (could not build the smaps probe)"; sed 's/^/    /' "$WORK/smapsdiag.log" | tee -a "$REPORT"
fi

say ""
kv "online cpus" "$(readv /sys/devices/system/cpu/online) (at end)"
kv "battery temp" "$(readv /sys/class/power_supply/battery/temp) (0.1 C) at end"
say "===== END OF REPORT ====="
echo
echo "보고서 파일: $REPORT"
echo "이 파일 내용을 그대로 복사해 전달하면 됩니다:  cat \"$REPORT\""
if command -v termux-clipboard-set >/dev/null 2>&1; then
    echo "(Termux:API 가 있으면:  termux-clipboard-set < \"$REPORT\"  로 한 번에 복사)"
fi
exit 0
