/* Three ways to leave nested loops after finding a value in a 2-D table.
   All three give the same answer; what differs is how easily a reader
   follows the flow. */
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

/* -- 1. a flag variable --
   The flag rides on the outer loop's condition. Plain standard C, but the
   condition grows and "where it ended" is scattered over two places. */
static struct pos find_flag(int target)
{
    struct pos p = { -1, -1, false };
    for (int i = 0; i < ROWS && !p.found; i++)
        for (int j = 0; j < COLS; j++)
            if (grid[i][j] == target) { p.row = i; p.col = j; p.found = true; break; }
    return p;
}

/* -- 2. goto --
   "shed both layers at once" shows in a single line.
   The label goes below, and is named for what it does. */
static struct pos find_goto(int target)
{
    struct pos p = { -1, -1, false };
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            if (grid[i][j] == target) { p.row = i; p.col = j; p.found = true; goto done; }
done:
    return p;
}

/* -- 3. lift it into a function and return --
   return sheds any number of layers at once; no escape device is needed. */
static struct pos find_return(int target)
{
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            if (grid[i][j] == target) return (struct pos){ i, j, true };
    return (struct pos){ -1, -1, false };
}

static void show(const char *how, struct pos p)
{
    if (p.found) printf("  %-18s (%d, %d)\n", how, p.row, p.col);
    else         printf("  %-18s not found\n", how);
}

int main(void)
{
    puts("[looking for 62 — the three ways agree]");
    show("1. flag", find_flag(62));
    show("2. goto", find_goto(62));
    show("3. function+return", find_return(62));

    puts("\n[looking for a value that is not there (100)]");
    show("1. flag", find_flag(100));
    show("2. goto", find_goto(100));
    show("3. function+return", find_return(100));

    puts("\n[break sheds one layer only — hence the devices above]");
    int visited = 0;
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++) { visited++; if (grid[i][j] == 62) break; }
    printf("  break in the inner loop only: visited %d cells (of %d)\n",
           visited, ROWS * COLS);
    puts("  -> only the inner loop ended; the outer one kept going.");
    return 0;
}
