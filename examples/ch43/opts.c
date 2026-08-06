/* 임시 구조체(복합 리터럴)로 "이름 붙은 매개변수"를 만들고,
   구조체 복사가 배열 멤버까지 옮긴다는 사실을 확인한다. */
#include <stdio.h>

/* ── ① 이름으로 넘기는 매개변수 ─────────────────────────── */
struct draw_opts {
    int         width;     /* 0 이면 기본값 80 */
    int         height;    /* 0 이면 기본값 24 */
    bool        grid;
    const char *title;
};

static void draw_(struct draw_opts o)
{
    int w = o.width  ? o.width  : 80;
    int h = o.height ? o.height : 24;
    printf("  %3dx%-3d grid=%-5s title=%s\n", w, h,
           o.grid ? "true" : "false", o.title ? o.title : "(없음)");
}
/* 호출자가 중괄호를 적지 않아도 되게 감싼다 */
#define draw(...) draw_((struct draw_opts){ __VA_ARGS__ })

/* ── ② 배열을 값으로 넘기기 ─────────────────────────────── */
struct row { int cell[8]; };          /* 배열을 구조체로 감싸면 값이 된다 */

static int total(struct row r)        /* 사본이 통째로 건너온다 */
{
    int t = 0;
    for (int i = 0; i < 8; i++) t += r.cell[i];
    r.cell[0] = 999;                  /* 사본만 바뀐다 */
    return t;
}

static int total_raw(int cell[8])     /* 배열 매개변수는 포인터로 무너진다 */
{
    int t = 0;
    for (int i = 0; i < 8; i++) t += cell[i];
    cell[0] = 999;                    /* 원본이 바뀐다 */
    return t;
}

int main(void)
{
    puts("이름으로 넘기기 (순서 무관, 빠뜨려도 됨)");
    draw(.title = "차트", .height = 20, .width = 40);
    draw(.grid = true);
    draw();                                     /* 전부 기본값 */

    /* 복합 리터럴은 주소를 얻을 수 있고, 수명은 이 블록의 끝까지다 */
    struct draw_opts *p = &(struct draw_opts){ .width = 5, .title = "임시" };
    printf("  임시 구조체의 주소로 접근: width=%d title=%s\n", p->width, p->title);

    puts("\n배열을 값으로 / 포인터로");
    struct row r = { .cell = { 1, 2, 3, 4, 5, 6, 7, 8 } };

    /* 호출과 원본 읽기를 반드시 갈라 적는다 - 한 표현식에 섞으면 순서가 없다 */
    int t1 = total(r);
    printf("  값으로  : 합 %2d,  호출 뒤 원본 cell[0] = %d\n", t1, r.cell[0]);
    int t2 = total_raw(r.cell);
    printf("  포인터로: 합 %2d,  호출 뒤 원본 cell[0] = %d\n", t2, r.cell[0]);

    /* 구조체 대입도 통째 복사다 — 배열 멤버까지 */
    struct row copy = r;
    copy.cell[1] = -1;
    printf("  대입 사본 수정 뒤 원본 cell[1] = %d, 사본 cell[1] = %d\n",
           r.cell[1], copy.cell[1]);
    return 0;
}
