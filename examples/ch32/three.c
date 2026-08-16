/* 같은 일을 세 형제로 각각 적어 보고, 서로 다른 자리를 드러낸다. */
#include <stdio.h>

/* 1부터 5까지 더하기 — while */
static int sum_while(void)
{
    int sum = 0;
    int i = 1;                  /* 시작: 루프 밖에 있다 */
    while (i <= 5) {            /* 조건 */
        sum += i;
        i += 1;                 /* 갱신: 몸통 끝에 있다 — 잊기 쉬운 자리 */
    }
    return sum;
}

/* 같은 일 — for. 살림(시작·조건·갱신)이 한 줄에 모인다 */
static int sum_for(void)
{
    int sum = 0;
    for (int i = 1; i <= 5; i += 1)
        sum += i;
    return sum;
}

/* 같은 일 — do-while. 몸통을 먼저 한 번 실행한다 */
static int sum_do(void)
{
    int sum = 0;
    int i = 1;
    do {
        sum += i;
        i += 1;
    } while (i <= 5);
    return sum;
}

/* 셋의 차이가 드러나는 자리 ① — 처음부터 조건이 거짓일 때 */
static int count_while(int n)
{
    int turns = 0, i = 0;
    while (i < n) { turns++; i++; }
    return turns;
}

static int count_do(int n)
{
    int turns = 0, i = 0;
    do { turns++; i++; } while (i < n);
    return turns;
}

/* 셋의 차이가 드러나는 자리 ② — continue 가 어디로 가는가.
   for 의 갱신식은 continue 뒤에도 반드시 실행된다. while 로 옮겨 적을 때
   갱신을 몸통 끝에 두면, continue 가 그 갱신을 뛰어넘어 루프가 멈추지 않는다. */
static int odd_sum_for(void)
{
    int sum = 0;
    for (int i = 1; i <= 9; i++) {
        if (i % 2 == 0)
            continue;           /* i++ 는 그래도 실행된다 */
        sum += i;
    }
    return sum;
}

static int odd_sum_while_fixed(void)
{
    int sum = 0;
    int i = 1;
    while (i <= 9) {
        if (i % 2 == 0) {
            i++;                /* 갱신을 여기서도 해 주어야 한다 */
            continue;
        }
        sum += i;
        i++;
    }
    return sum;
}

/* do-while 이 정말로 값을 하는 자리 — 부호 없는 첨자로 거꾸로 걷기.
   size_t 는 0 아래로 내려가지 못하므로 for (size_t i = n-1; i >= 0; i--) 는
   무한 루프가 된다(41장). do-while 은 「먼저 하나 줄이고, 0 을 처리한 뒤 끝낸다」
   를 그대로 적을 수 있다. 단, 몸통이 먼저 도는 형태이므로 n 이 0 이 아니어야
   한다 — 그 검사가 이 무늬의 값과 짝을 이룬다. */
static void countdown(size_t n)
{
    if (n == 0) {                   /* do-while 의 「최소 한 번」을 막아 준다 */
        printf("  (nothing to visit)\n");
        return;
    }
    printf("  ");
    size_t i = n;
    do {
        i--;                        /* n-1 부터 0 까지 */
        printf("%s%zu", i == n - 1 ? "" : " ", i);
    } while (i > 0);
    printf("\n");
}

int main(void)
{
    printf("[the same job, three ways]\n");
    printf("  while    : 1..5 -> %d\n", sum_while());
    printf("  for      : 1..5 -> %d\n", sum_for());
    printf("  do-while : 1..5 -> %d\n", sum_do());

    printf("\n[what changes when the condition is false from the start]\n");
    printf("  while (i < 0) ran %d time(s)\n", count_while(0));
    printf("  do ... while (i < 0) ran %d time(s)\n", count_do(0));

    printf("\n[continue and the update step]\n");
    printf("  for   : sum of odd numbers 1..9 = %d\n", odd_sum_for());
    printf("  while : sum of odd numbers 1..9 = %d\n", odd_sum_while_fixed());
    printf("\n[walking backwards with an unsigned index]\n");
    countdown(6);
    countdown(1);
    countdown(0);
    printf("  do-while says it plainly: step down first, handle 0, then stop.\n");

    printf("  in the for loop i++ runs even after continue;\n");
    printf("  in the while loop the update sits in the body, so continue can skip it -\n");
    printf("  that is how a working loop turns into an endless one when it is rewritten.\n");
    return 0;
}
