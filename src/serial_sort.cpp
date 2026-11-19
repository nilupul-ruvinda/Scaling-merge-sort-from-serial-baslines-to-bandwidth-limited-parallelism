#include "sort_variants.hpp"
#include <algorithm>

namespace {
// Classical linear two-pointer merge pass
void merge_serial(int64_t* arr, int64_t* aux, size_t left, size_t mid, size_t right) {
    size_t i = left;
    size_t j = mid + 1;
    size_t k = left;

    // Stream the smaller element into the auxiliary scratchpad
    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) {
            aux[k++] = arr[i++];
        } else {
            aux[k++] = arr[j++];
        }
    }

    // Flush any remaining elements from the left partition
    while (i <= mid) {
        aux[k++] = arr[i++];
    }

    // Flush any remaining elements from the right partition
    while (j <= right) {
        aux[k++] = arr[j++];
    }

    // Copy merged segment back to the working array
    for (size_t p = left; p <= right; ++p) {
        arr[p] = aux[p];
    }
}
} // anonymous namespace

void serial_merge_sort(int64_t* arr, int64_t* aux, size_t left, size_t right) {
    if (left >= right) {
        return;
    }

    // Leaf cutoff optimization
    if (right - left + 1 <= SERIAL_LEAF_CUTOFF) {
        std::sort(arr + left, arr + right + 1);
        return;
    }

    size_t mid = left + (right - left) / 2;

    serial_merge_sort(arr, aux, left, mid);
    serial_merge_sort(arr, aux, mid + 1, right);

    merge_serial(arr, aux, left, mid, right);
}
