#include "sort_variants.hpp"
#include <algorithm>
#include <omp.h>

namespace {

constexpr size_t PARALLEL_MERGE_CUTOFF = 2048;

void merge_serial_to_dest(const int64_t* src, int64_t* dst,
                          size_t left1, size_t right1,
                          size_t left2, size_t right2,
                          size_t dst_left) {
    size_t i = left1;
    size_t j = left2;
    size_t k = dst_left;

    while (i <= right1 && j <= right2) {
        if (src[i] <= src[j]) {
            dst[k++] = src[i++];
        } else {
            dst[k++] = src[j++];
        }
    }
    while (i <= right1) dst[k++] = src[i++];
    while (j <= right2) dst[k++] = src[j++];
}

// Blelloch concurrent merge using half-open intervals [left, right)
void parallel_merge(const int64_t* src, int64_t* dst,
                    size_t left1, size_t right1,
                    size_t left2, size_t right2,
                    size_t dst_left, int depth) {
    size_t len1 = right1 - left1;
    size_t len2 = right2 - left2;

    // Ensure the first range is always the larger one
    if (len1 < len2) {
        parallel_merge(src, dst, left2, right2, left1, right1, dst_left, depth);
        return;
    }

    if (len1 == 0) {
        return;
    }

    // Leaf cutoff to serial merge
    if ((len1 + len2) <= PARALLEL_MERGE_CUTOFF || depth > 8) {
        if (len2 == 0) {
            std::copy(src + left1, src + right1, dst + dst_left);
        } else {
            merge_serial_to_dest(src, dst, left1, right1 - 1, left2, right2 - 1, dst_left);
        }
        return;
    }

    // Bisect larger partition
    size_t mid1 = left1 + len1 / 2;
    int64_t pivot = src[mid1];

    // Binary search in second partition
    const int64_t* start2 = src + left2;
    const int64_t* end2 = src + right2;
    const int64_t* it = std::lower_bound(start2, end2, pivot);
    size_t mid2 = left2 + std::distance(start2, it);

    // Calculate destination offset for pivot
    size_t offset = dst_left + (mid1 - left1) + (mid2 - left2);
    dst[offset] = pivot;

    // Concurrently merge disjoint partitions
    #pragma omp task shared(src, dst)
    parallel_merge(src, dst, left1, mid1, left2, mid2, dst_left, depth + 1);

    #pragma omp task shared(src, dst)
    parallel_merge(src, dst, mid1 + 1, right1, mid2, right2, offset + 1, depth + 1);

    #pragma omp taskwait
}

} // anonymous namespace

void omp_blelloch_merge_sort(int64_t* arr, int64_t* aux, size_t left, size_t right, int depth) {
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
        omp_blelloch_merge_sort(arr, aux, left, mid, depth + 1);

        #pragma omp task shared(arr, aux)
        omp_blelloch_merge_sort(arr, aux, mid + 1, right, depth + 1);

        #pragma omp taskwait
    } else {
        omp_blelloch_merge_sort(arr, aux, left, mid, depth + 1);
        omp_blelloch_merge_sort(arr, aux, mid + 1, right, depth + 1);
    }

    // Pass half-open intervals [left, mid + 1) and [mid + 1, right + 1)
    parallel_merge(arr, aux, left, mid + 1, mid + 1, right + 1, left, 0);

    for (size_t p = left; p <= right; ++p) {
        arr[p] = aux[p];
    }
}
