/* 2차원 표에서 값을 찾아 다중 루프를 빠져나오는 세 가지 방식.
   셋은 같은 답을 내고, 다른 것은 '읽는 사람이 흐름을 얼마나 쉽게 따라가는가'다. */
#include <stdbool.h>
#include <stdio.h>

#define ROWS 4
#define COLS 5

static const int grid[ROWS][COLS] = {
    {  3, 14, 15,  92,  6 },
    { 53, 58,  9,  79,  3 },
    { 23, 84, 62,  64, 33 },
    { 83, 27, 95,  28, 84 },
};

struct pos { int row, col; bool found; };

/* ── ① 플래그 변수 ──
   바깥 루프의 조건에 플래그를 얹는다. 표준 C 만으로 되지만,
   조건이 늘고 '어디서 끝났는가'가 두 곳에 흩어진다. */
static struct pos find_flag(int target)
{
    struct pos p = { -1, -1, false };
    for (int i = 0; i < ROWS && !p.found; i++)
        for (int j = 0; j < COLS; j++)
            if (grid[i][j] == target) { p.row = i; p.col = j; p.found = true; break; }
    return p;
}

/* ── ② goto ──
   '두 겹을 한 번에 벗는다'는 뜻이 한 줄로 드러난다.
   레이블은 아래에 두고, 이름은 하는 일로 짓는다. */
static struct pos find_goto(int target)
{
    struct pos p = { -1, -1, false };
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            if (grid[i][j] == target) { p.row = i; p.col = j; p.found = true; goto done; }
done:
    return p;
}

/* ── ③ 함수로 빼고 return ──
   return 은 몇 겹이든 한 번에 벗는다. 탈출 장치가 아예 필요 없어진다. */
static struct pos find_return(int target)
{
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            if (grid[i][j] == target) return (struct pos){ i, j, true };
    return (struct pos){ -1, -1, false };
}

static void show(const char *how, struct pos p)
{
    if (p.found) printf("  %-16s (%d, %d)\n", how, p.row, p.col);
    else         printf("  %-16s 없음\n", how);
}

int main(void)
{
    puts("[62 를 찾는다 — 세 방식이 같은 답을 낸다]");
    show("① 플래그", find_flag(62));
    show("② goto",   find_goto(62));
    show("③ 함수+return", find_return(62));

    puts("\n[없는 값(100)을 찾으면]");
    show("① 플래그", find_flag(100));
    show("② goto",   find_goto(100));
    show("③ 함수+return", find_return(100));

    puts("\n[break 는 한 겹만 벗는다 — 그래서 위의 장치가 필요하다]");
    int visited = 0;
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++) { visited++; if (grid[i][j] == 62) break; }
    printf("  안쪽에서만 break: 칸을 %d 개 훑었다 (전체 %d 개)\n",
           visited, ROWS * COLS);
    puts("  → 안쪽 루프만 끝났을 뿐 바깥 루프는 계속 돌았다.");
    return 0;
}
