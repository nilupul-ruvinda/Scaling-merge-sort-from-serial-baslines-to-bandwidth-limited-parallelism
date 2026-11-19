#include "sort_variants.hpp"
#include <algorithm>
#include <omp.h>

namespace {
// Classical sequential linear merge pass
void merge_serial(int64_t* arr, int64_t* aux, size_t left, size_t mid, size_t right) {
    size_t i = left;
    size_t j = mid + 1;
    size_t k = left;

    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) {
            aux[k++] = arr[i++];
        } else {
            aux[k++] = arr[j++];
        }
    }

    while (i <= mid) {
        aux[k++] = arr[i++];
    }

    while (j <= right) {
        aux[k++] = arr[j++];
    }

    for (size_t p = left; p <= right; ++p) {
        arr[p] = aux[p];
    }
}
} // anonymous namespace

void omp_divide_merge_sort(int64_t* arr, int64_t* aux, size_t left, size_t right, int depth) {
    if (left >= right) {
        return;
    }

    if (right - left + 1 <= SERIAL_LEAF_CUTOFF) {
        std::sort(arr + left, arr + right + 1);
        return;
    }

    size_t mid = left + (right - left) / 2;

    if (depth <= 4) {
        #pragma omp task shared(arr, aux)
        omp_divide_merge_sort(arr, aux, left, mid, depth + 1);

        #pragma omp task shared(arr, aux)
        omp_divide_merge_sort(arr, aux, mid + 1, right, depth + 1);

        #pragma omp taskwait
    } else {
        omp_divide_merge_sort(arr, aux, left, mid, depth + 1);
        omp_divide_merge_sort(arr, aux, mid + 1, right, depth + 1);
    }

    // Single-threaded linear merge creates Amdahl's plateau
    merge_serial(arr, aux, left, mid, right);
}
