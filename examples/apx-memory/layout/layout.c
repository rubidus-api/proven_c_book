/* 배치도는 막는 장치이자 *쓰는* 장치다.

   셋을 보인다.
     ① 같은 물리 기억을 주소 공간에 두 번 이어 붙여 「감기지 않는 고리 버퍼」를 만든다.
     ② 마지막 쪽의 권한을 거두어 경비 쪽으로 삼는다 --- 넘겨 쓰면 거기서 멈춘다.
     ③ 이 프로그램 자신의 사상 목록에서 권한 조합을 세어 본다. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/mman.h>

static sigjmp_buf back;

static void on_segv(int sig)
{
    (void)sig;
    siglongjmp(back, 1);
}

static void ring_of_one_page(size_t pg)
{
    int fd = memfd_create("ring", 0);
    if (fd < 0 || ftruncate(fd, (long)pg) != 0) { puts("  (memfd unavailable)"); return; }

    /* 먼저 두 쪽 넓이의 자리를 잡아 두고, 그 위에 같은 기억을 두 번 덮어 사상한다. */
    char *base = mmap(NULL, pg * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    char *a = mmap(base,      pg, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    char *b = mmap(base + pg, pg, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    if (a == MAP_FAILED || b == MAP_FAILED) { puts("  (mapping refused)"); return; }

    strcpy(a, "hello");
    printf("  two windows %zu bytes apart; wrote at the first, read \"%s\" at the second\n",
           (size_t)(b - a), b);

    /* 끝에서 8 바이트 앞에 12 바이트를 쓴다 --- 감기 검사 없이 그냥 이어 쓴다. */
    memcpy(a + pg - 8, "ABCDEFGHIJKL", 12);
    printf("  wrote 12 bytes 8 before the end; the last 4 came out at the start: \"%.4s\"\n", a);
    printf("#DATA ring %.4s\n", a);
    close(fd);
}

static void guard_page(size_t pg)
{
    struct sigaction sa;
    char *p = mmap(NULL, pg * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { puts("  (mapping refused)"); return; }
    mprotect(p + pg * 2, pg, PROT_NONE);            /* 마지막 쪽의 권한을 거둔다 */

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_segv;
    sigaction(SIGSEGV, &sa, NULL);

    if (sigsetjmp(back, 1) == 0) {
        for (size_t i = 0; i < pg * 3; i++) p[i] = 1;
        puts("  the overrun ran past the guard -- nothing stopped it");
        printf("#DATA guard none\n");
    } else {
        printf("  the overrun stopped at the guard page, %zu bytes in\n", pg * 2);
        printf("#DATA guard stopped\n");
    }
    signal(SIGSEGV, SIG_DFL);
}

static void permissions_of_this_program(void)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512], perm[8];
    long r = 0, x = 0, w = 0, none = 0, wx = 0, total = 0;
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%*s %7s", perm) != 1) continue;
        total++;
        if (perm[0] == 'r' && perm[2] == 'x') x++;
        else if (perm[1] == 'w') w++;
        else if (perm[0] == 'r') r++;
        if (perm[0] == '-' && perm[1] == '-' && perm[2] == '-') none++;
        if (perm[1] == 'w' && perm[2] == 'x') wx++;
    }
    fclose(f);
    printf("  %ld regions: %ld read-only, %ld readable+writable, %ld executable, %ld with no access\n",
           total, r, w, x, none);
    printf("  regions that are writable AND executable: %ld\n", wx);
    printf("#DATA regions %ld\n", total);
    printf("#DATA wx %ld\n", wx);
}

int main(void)
{
    size_t pg = (size_t)sysconf(_SC_PAGESIZE);
    void *both;

    printf("== the page size of this machine: %zu bytes ==\n\n", pg);
    printf("#DATA-BEGIN\n");
    printf("#DATA pagesize %zu\n", pg);

    puts("-- one page of memory, mapped twice in a row --");
    ring_of_one_page(pg);

    puts("\n-- a page with its permissions taken away --");
    guard_page(pg);

    puts("\n-- the permissions this program is running under --");
    permissions_of_this_program();

    both = mmap(NULL, pg, PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("  asking for one that is both writable and executable: %s\n",
           both == MAP_FAILED ? "refused" : "granted");
    printf("#DATA wxrequest %s\n", both == MAP_FAILED ? "refused" : "granted");
    printf("#DATA-END\n");

    puts("\n  * the same memory can sit at two addresses, and an address can have no");
    puts("    memory at all. What a pointer names is a place on a map.");
    return 0;
}
