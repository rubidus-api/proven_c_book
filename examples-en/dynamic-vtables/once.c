/* Build once, publish once: eight threads ask for the tables at the same moment.
   call_once runs the builder exactly once, and every caller returns only after it
   has finished --- so no thread can see a half-filled table. */
#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

struct ops { int (*area)(int); int (*perimeter)(int); };
static int sq_area(int s) { return s * s; }
static int sq_perimeter(int s) { return 4 * s; }

static struct ops table;                /* filled at run time, then only read */
static once_flag table_once = ONCE_FLAG_INIT;
static atomic_int builds;

static void build(void)
{
    atomic_fetch_add(&builds, 1);
    table.area = sq_area;
    thrd_sleep(&(struct timespec){ .tv_nsec = 20'000'000 }, nullptr);  /* a slow build */
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
    answer[i] = seen[i]->area(3) + seen[i]->perimeter(3);  /* both slots are there */
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
