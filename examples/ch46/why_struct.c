#include <stdio.h>

/* 구조체 없이: 같은 학생의 정보가 세 배열에 흩어져 있다 */
static const char *names[3]  = {"Ada", "Ken", "Bjarne"};
static int         years[3]  = {1815, 1943, 1950};
static double      scores[3] = {97.5, 88.0, 91.0};

/* 구조체로: 한 학생의 정보가 한 덩어리다 */
struct student {
    const char *name;
    int         year;
    double      score;
};

static struct student best(const struct student *v, size_t n)
{
    struct student top = v[0];
    for (size_t i = 1; i < n; i++)
        if (v[i].score > top.score)
            top = v[i];                 /* 통째로 대입 --- 세 멤버가 함께 */
    return top;                         /* 값 셋을 한 번에 돌려준다 */
}

int main(void)
{
    puts("[1] parallel arrays: one student, three places");
    /* 점수만 정렬하다 이름을 같이 옮기는 것을 잊으면 짝이 어긋난다 */
    double t = scores[0]; scores[0] = scores[1]; scores[1] = t;
    printf("%s: %.1f   <- the score moved, the name did not\n",
           names[0], scores[0]);
    (void)years;

    puts("\n[2] a structure keeps one student in one piece");
    struct student cls[3] = {
        {.name = "Ada",    .year = 1815, .score = 97.5},
        {.name = "Ken",    .year = 1943, .score = 88.0},
        {.name = "Bjarne", .year = 1950, .score = 91.0},
    };
    struct student tmp = cls[0]; cls[0] = cls[1]; cls[1] = tmp;
    printf("%s: %.1f   <- name and score moved together\n",
           cls[0].name, cls[0].score);

    puts("\n[3] a function can return all three at once");
    struct student b = best(cls, 3);
    printf("best: %s (%d) %.1f\n", b.name, b.year, b.score);

    puts("\n[4] members lie in declaration order");
    printf("sizeof(struct student) = %zu bytes\n", sizeof(struct student));
    printf("&b == &b.name : %d\n", (void *)&b == (void *)&b.name);
    printf("&b.name < &b.year < &b.score : %d\n",
           (void *)&b.name < (void *)&b.year
           && (void *)&b.year < (void *)&b.score);
    return 0;
}
