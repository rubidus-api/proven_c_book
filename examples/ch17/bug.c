#include <stdio.h>

static int sum_all(const int *a, int n) {
    int s = 0;
    for (int i = 0; i < n - 1; i++)   /* bug: stops one short */
        s += a[i];
    return s;
}

int main(void) {
    int data[5] = {10, 20, 30, 40, 50};
    printf("sum = %d (should be 150)\n", sum_all(data, 5));
    return 0;
}
