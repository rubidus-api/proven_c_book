#include "proven/algorithm.h"
#include "../../platform/proven_sys_mem.h"

// -------------------------------------------------------------
// Internal Sort Implementation
// -------------------------------------------------------------
//
// A Lomuto partition with a strict `cmp(x, pivot) < 0` test sends every element
// EQUAL to the pivot into the right partition. On low-cardinality keys - a
// status column, an enum, a bucket id, anything a caller is likely to sort by -
// that degrades to O(n^2): sorting 100k identical int32 keys took 10.6 seconds.
// A caller who sorts data an attacker can shape has a denial of service.
//
// So: a three-way (Bentley-McIlroy) partition, which puts the equal run in the
// middle and never recurses into it - all-equal input becomes O(n) - plus an
// insertion-sort cutoff for small ranges, and a heapsort fallback once the
// recursion goes deeper than 2*log2(n). The fallback is what turns "fast in
// practice" into an O(n log n) guarantee: median-of-three alone can still be
// driven quadratic by an adversarial ordering, and this library does not get to
// hand its callers a worst case it cannot name.

#define PROVEN_SORT_INSERTION_CUTOFF ((proven_size_t)12)
#define PROVEN_SORT_SCRATCH ((proven_size_t)128)

static void swap_elements(proven_byte_t *a, proven_byte_t *b, proven_size_t size) {
    if (a == b) return;

    // Three bulk copies beat 3*size byte loads/stores once the element is more
    // than a few bytes. Below that the call overhead dominates - proven_sys_mem_copy
    // lives in another translation unit and cannot be inlined - so tiny elements
    // keep the byte loop.
    if (size > 16 && size <= PROVEN_SORT_SCRATCH) {
        proven_byte_t tmp[PROVEN_SORT_SCRATCH];
        proven_sys_mem_copy(tmp, a, size);
        proven_sys_mem_copy(a, b, size);
        proven_sys_mem_copy(b, tmp, size);
        return;
    }

    for (proven_size_t i = 0; i < size; ++i) {
        proven_byte_t tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

static proven_byte_t *elem_at(proven_byte_t *base, proven_size_t i, proven_size_t size) {
    return base + (i * size);
}

/* The scratch below is handed to the CALLER'S comparator, which will read it as the
 * element type. A plain `proven_byte_t[128]` has alignment 1, so for any element more
 * strictly aligned than the compiler happened to place that array, the comparator makes
 * a misaligned typed access: undefined behaviour, and a fault on a strict-alignment
 * target. UBSan flags it at every optimisation level for an over-aligned struct. Over-
 * aligning the scratch fixes it up to this bound; beyond it, insertion_sort takes the
 * swap path, which only ever shows the comparator real array elements. (swap_elements'
 * own scratch never reaches the comparator - memcpy only - so it needs nothing.) */
#define PROVEN_SORT_SCRATCH_ALIGN ((proven_size_t)64)

// Shift, do not swap: a swap-per-step insertion sort writes each displaced
// element once per position it moves past.
static void insertion_sort(proven_byte_t *base, proven_size_t n, proven_size_t size,
                           proven_size_t align, proven_compare_fn_t cmp) {
    if (n < 2) return;

    if (size <= PROVEN_SORT_SCRATCH && align <= PROVEN_SORT_SCRATCH_ALIGN) {
        alignas(PROVEN_SORT_SCRATCH_ALIGN) proven_byte_t tmp[PROVEN_SORT_SCRATCH];
        for (proven_size_t i = 1; i < n; ++i) {
            proven_byte_t *cur = elem_at(base, i, size);
            if (cmp(cur, elem_at(base, i - 1, size)) >= 0) continue;

            proven_sys_mem_copy(tmp, cur, size);
            proven_size_t j = i;
            while (j > 0 && cmp(elem_at(base, j - 1, size), tmp) > 0) {
                proven_sys_mem_copy(elem_at(base, j, size), elem_at(base, j - 1, size), size);
                --j;
            }
            proven_sys_mem_copy(elem_at(base, j, size), tmp, size);
        }
        return;
    }

    // Elements too large for the scratch buffer: fall back to swapping.
    for (proven_size_t i = 1; i < n; ++i) {
        for (proven_size_t j = i; j > 0; --j) {
            proven_byte_t *cur = elem_at(base, j, size);
            proven_byte_t *prev = elem_at(base, j - 1, size);
            if (cmp(cur, prev) >= 0) break;
            swap_elements(cur, prev, size);
        }
    }
}

static proven_size_t median3(proven_byte_t *base, proven_size_t a, proven_size_t b, proven_size_t c,
                             proven_size_t size, proven_compare_fn_t cmp) {
    int ab = cmp(elem_at(base, a, size), elem_at(base, b, size));
    int bc = cmp(elem_at(base, b, size), elem_at(base, c, size));
    int ac = cmp(elem_at(base, a, size), elem_at(base, c, size));
    if (ab < 0) {
        if (bc < 0) return b;
        return (ac < 0) ? c : a;
    }
    if (bc > 0) return b;
    return (ac < 0) ? a : c;
}

static void sift_down(proven_byte_t *base, proven_size_t root, proven_size_t n,
                      proven_size_t size, proven_compare_fn_t cmp) {
    for (;;) {
        proven_size_t child = 2 * root + 1;
        if (child >= n) break;
        if (child + 1 < n && cmp(elem_at(base, child, size), elem_at(base, child + 1, size)) < 0) {
            ++child;
        }
        if (cmp(elem_at(base, root, size), elem_at(base, child, size)) >= 0) break;
        swap_elements(elem_at(base, root, size), elem_at(base, child, size), size);
        root = child;
    }
}

// The introsort escape hatch: O(n log n) no matter what the input does.
static void heapsort_range(proven_byte_t *base, proven_size_t n, proven_size_t size, proven_compare_fn_t cmp) {
    if (n < 2) return;
    for (proven_size_t i = n / 2; i > 0; --i) {
        sift_down(base, i - 1, n, size, cmp);
    }
    for (proven_size_t end = n - 1; end > 0; --end) {
        swap_elements(base, elem_at(base, end, size), size);
        sift_down(base, 0, end, size, cmp);
    }
}

// Swap `count` elements between two non-overlapping runs.
static void vec_swap(proven_byte_t *a, proven_byte_t *b, proven_size_t count, proven_size_t size) {
    for (proven_size_t i = 0; i < count; ++i) {
        swap_elements(a + i * size, b + i * size, size);
    }
}

static void introsort(proven_byte_t *base, proven_size_t n, proven_size_t size,
                      proven_size_t align, proven_compare_fn_t cmp, proven_size_t depth_budget) {
    while (n > PROVEN_SORT_INSERTION_CUTOFF) {
        if (depth_budget == 0) {
            heapsort_range(base, n, size, cmp);
            return;
        }
        --depth_budget;

        proven_size_t m = median3(base, 0, n / 2, n - 1, size, cmp);
        swap_elements(base, elem_at(base, m, size), size);

        // Bentley-McIlroy partition. Equal elements are parked at the two ends as
        // they are met and swept into the middle at the end, so every element is
        // compared exactly once - the reason to prefer this over the obvious
        // three-way loop, which re-compares whatever it moves to the right.
        //
        //   [ == | <  |  unexamined  |  > | == ]
        //    ^pa  ^pb                 ^pc  ^pd
        proven_size_t pa = 1, pb = 1;
        proven_size_t pc = n - 1, pd = n - 1;
        for (;;) {
            int r;
            while (pb <= pc && (r = cmp(elem_at(base, pb, size), base)) <= 0) {
                if (r == 0) {
                    swap_elements(elem_at(base, pa, size), elem_at(base, pb, size), size);
                    ++pa;
                }
                ++pb;
            }
            while (pb <= pc && (r = cmp(elem_at(base, pc, size), base)) >= 0) {
                if (r == 0) {
                    swap_elements(elem_at(base, pc, size), elem_at(base, pd, size), size);
                    --pd;
                }
                --pc;
            }
            if (pb > pc) break;
            swap_elements(elem_at(base, pb, size), elem_at(base, pc, size), size);
            ++pb;
            --pc;
        }

        // The loop leaves the array as
        //
        //   [ ==  |  <   |   >   |  ==  ]
        //   0..pa-1  pa..pb-1  pc+1..pd  pd+1..n-1
        //
        // so the equal elements sit at the two ENDS and have to be swept into the
        // middle. Each sweep exchanges as many elements as the shorter of the two
        // runs it is joining - swapping more would run off the end of one of them.
        proven_size_t left_equal  = pa;                   /* == parked at the front */
        proven_size_t left_less   = pb - pa;              /* strictly less than pivot */
        proven_size_t right_more  = pd - pc;              /* strictly greater than pivot */
        proven_size_t right_equal = n - 1 - pd;           /* == parked at the back */

        proven_size_t s;
        s = (left_equal < left_less) ? left_equal : left_less;
        vec_swap(base, elem_at(base, pb - s, size), s, size);

        s = (right_equal < right_more) ? right_equal : right_more;
        vec_swap(elem_at(base, pb, size), elem_at(base, n - s, size), s, size);

        proven_size_t left_n  = left_less;
        proven_size_t right_n = right_more;

        // Elements equal to the pivot are now in the middle and final; never
        // recurse into them. All-equal input therefore costs one pass, not n.
        if (left_n < right_n) {
            introsort(base, left_n, size, align, cmp, depth_budget);
            base = elem_at(base, n - right_n, size);
            n = right_n;
        } else {
            introsort(elem_at(base, n - right_n, size), right_n, size, align, cmp, depth_budget);
            n = left_n;
        }
    }
    insertion_sort(base, n, size, align, cmp);
}

static proven_size_t depth_limit(proven_size_t n) {
    proven_size_t levels = 0;
    while (n > 1) {
        n >>= 1;
        ++levels;
    }
    return 2 * levels + 1;   // 2*floor(log2(n)), never 0
}

// -------------------------------------------------------------
// Public C API
// -------------------------------------------------------------

void proven_array_sort(proven_array_t *arr, proven_compare_fn_t cmp) {
    if (!arr || arr->len < 2 || !cmp || arr->elem_size == 0) return;
    introsort((proven_byte_t*)arr->data, arr->len, arr->elem_size,
              arr->align ? arr->align : (proven_size_t)1, cmp, depth_limit(arr->len));
}

void* proven_array_binary_search(const proven_array_t *arr, const void *key, proven_compare_fn_t cmp) {
    if (!arr || arr->len == 0 || !cmp || !key) return (void*)0;

    proven_ptrdiff_t low = 0;
    proven_ptrdiff_t high = (proven_ptrdiff_t)arr->len - 1;
    proven_byte_t *base = (proven_byte_t*)arr->data;

    while (low <= high) {
        proven_ptrdiff_t mid = low + (high - low) / 2;
        void *mid_ptr = base + ((proven_size_t)mid * arr->elem_size);
        int res = cmp(key, mid_ptr);

        if (res == 0) return mid_ptr;
        if (res < 0) high = mid - 1;
        else low = mid + 1;
    }

    return (void*)0;
}

void* proven_array_linear_search(const proven_array_t *arr, const void *key, proven_compare_fn_t cmp) {
    if (!arr || !key || !cmp) return (void*)0;
    
    proven_byte_t *base = (proven_byte_t*)arr->data;
    for (proven_size_t i = 0; i < arr->len; ++i) {
        void *current = base + (i * arr->elem_size);
        if (cmp(key, current) == 0) return current;
    }
    
    return (void*)0;
}
