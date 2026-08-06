/* Making "named parameters" out of a temporary struct (a compound literal),
   and confirming that copying a struct carries an array member along too. */
#include <stdio.h>

/* ── (1) parameters passed by name ───────────────────────── */
struct draw_opts {
    int         width;     /* 0 means the default, 80 */
    int         height;    /* 0 means the default, 24 */
    bool        grid;
    const char *title;
};

static void draw_(struct draw_opts o)
{
    int w = o.width  ? o.width  : 80;
    int h = o.height ? o.height : 24;
    printf("  %3dx%-3d grid=%-5s title=%s\n", w, h,
           o.grid ? "true" : "false", o.title ? o.title : "(none)");
}
/* wrapped so the caller need not write the braces */
#define draw(...) draw_((struct draw_opts){ __VA_ARGS__ })

/* ── (2) passing an array by value ───────────────────────── */
struct row { int cell[8]; };          /* wrap an array in a struct and it becomes a value */

static int total(struct row r)        /* the whole copy comes across */
{
    int t = 0;
    for (int i = 0; i < 8; i++) t += r.cell[i];
    r.cell[0] = 999;                  /* only the copy changes */
    return t;
}

static int total_raw(int cell[8])     /* an array parameter decays to a pointer */
{
    int t = 0;
    for (int i = 0; i < 8; i++) t += cell[i];
    cell[0] = 999;                    /* the original changes */
    return t;
}

int main(void)
{
    puts("passing by name (order does not matter, and any may be left out)");
    draw(.title = "chart", .height = 20, .width = 40);
    draw(.grid = true);
    draw();                                     /* everything default */

    /* a compound literal has an address, and its lifetime runs to the end of this block */
    struct draw_opts *p = &(struct draw_opts){ .width = 5, .title = "temporary" };
    printf("  reached through the temporary's address: width=%d title=%s\n", p->width, p->title);

    puts("\nan array by value / by pointer");
    struct row r = { .cell = { 1, 2, 3, 4, 5, 6, 7, 8 } };

    /* the call and the reading of the original are written apart on purpose -
       mix them in one expression and there is no order between them */
    int t1 = total(r);
    printf("  by value  : sum %2d,  after the call cell[0] = %d\n", t1, r.cell[0]);
    int t2 = total_raw(r.cell);
    printf("  by pointer: sum %2d,  after the call cell[0] = %d\n", t2, r.cell[0]);

    /* struct assignment is a whole copy too — array members included */
    struct row copy = r;
    copy.cell[1] = -1;
    printf("  after changing the copy: original cell[1] = %d, copy cell[1] = %d\n",
           r.cell[1], copy.cell[1]);
    return 0;
}
