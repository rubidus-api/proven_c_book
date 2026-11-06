/* 한 번 짓고 한 번 내놓는다: 스레드 여덟이 한꺼번에 표를 달라고 한다.
   call_once 는 짓는 함수를 정확히 한 번 돌리고, 부른 쪽은 모두 그것이 끝난 뒤에야
   돌아온다 --- 그래서 반쯤 채운 표를 보는 스레드가 없다. */
#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

struct ops { int (*area)(int); int (*perimeter)(int); };
static int sq_area(int s) { return s * s; }
static int sq_perimeter(int s) { return 4 * s; }

static struct ops table;                /* 실행 때 채우고, 그 뒤로는 읽기만 */
static once_flag table_once = ONCE_FLAG_INIT;
static atomic_int builds;

static void build(void)
{
    atomic_fetch_add(&builds, 1);
    table.area = sq_area;
    thrd_sleep(&(struct timespec){ .tv_nsec = 20'000'000 }, nullptr);  /* 느린 짓기 */
    table.perimeter = sq_perimeter;
}
static const struct ops *get_table(void)
{
    call_once(&table_once, build);
    return &table;
}

enum { THREADS = 8 };
static const struct ops *seen[THREADS];
static int answer[THREADS];
static int worker(void *arg)
{
    int i = *(int *)arg;
    seen[i] = get_table();
    answer[i] = seen[i]->area(3) + seen[i]->perimeter(3);  /* 두 칸 다 있다 */
    return 0;
}

int main(void)
{
    thrd_t t[THREADS];
    int id[THREADS];
    for (int i = 0; i < THREADS; i++) {
        id[i] = i;
        if (thrd_create(&t[i], worker, &id[i]) != thrd_success) {
            puts("thrd_create failed");
            return 1;
        }
    }
    for (int i = 0; i < THREADS; i++) thrd_join(t[i], nullptr);

    bool same = true, right = true;
    for (int i = 0; i < THREADS; i++) {
        same = same && seen[i] == seen[0];
        right = right && answer[i] == 9 + 12;
    }
    printf("%d threads asked; the builder ran %d time(s)\n",
           THREADS, atomic_load(&builds));
    printf("every thread got the same table: %s\n", same ? "yes" : "no");
    printf("every answer was area 9 + perimeter 12 = 21, both slots filled: %s\n",
           right ? "yes" : "no");
    return 0;
}
