/* 결함이 있던 판 --- 마지막 칸을 못 본다(`lo < hi` 로 적은 흔한 실수) */
#include "find.h"

int find(const int *a, int n, int key)
{
    int lo = 0, hi = n - 1;
    while (lo < hi) {              /* ★ 여기가 결함이다 --- `<=` 여야 한다 */
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == key) return mid;
        if (a[mid] < key)  lo = mid + 1;
        else               hi = mid - 1;
    }
    return -1;
}
