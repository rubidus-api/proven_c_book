/* the version with the defect - it cannot see the last cell (`lo < hi`, a common slip) */
#include "find.h"

int find(const int *a, int n, int key)
{
    int lo = 0, hi = n - 1;
    while (lo < hi) {              /* * here is the defect - it must be `<=` */
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == key) return mid;
        if (a[mid] < key)  lo = mid + 1;
        else               hi = mid - 1;
    }
    return -1;
}
