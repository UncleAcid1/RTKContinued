// StlportSort: std::sort as STLport 5 implements it (the library the game was built with), so that
// elements with equal keys end up in the same order as on the original. Introsort: median-of-three
// pivot (first, middle, last - 1), unguarded partition, heap sort past 2 * log2(n) levels, segments
// of up to 16 left for a final insertion sort. Matches the instances inlined in ShopWindow::Init
// (FUN_00348d9c / FUN_00348994 / FUN_00348a38).
#pragma once
#include <cstddef>
#include <utility>

namespace StlSort {

template <class T, class Less>
void PushHeap(T* first, ptrdiff_t hole, ptrdiff_t top, T value, Less less) {
    ptrdiff_t parent = (hole - 1) / 2;
    while (hole > top && less(first[parent], value)) {
        first[hole] = first[parent];
        hole = parent;
        parent = (hole - 1) / 2;
    }
    first[hole] = value;
}

template <class T, class Less>
void AdjustHeap(T* first, ptrdiff_t hole, ptrdiff_t len, T value, Less less) {
    ptrdiff_t top = hole, child = 2 * hole + 2;
    while (child < len) {
        if (less(first[child], first[child - 1])) --child;
        first[hole] = first[child];
        hole = child;
        child = 2 * (child + 1);
    }
    if (child == len) {
        first[hole] = first[child - 1];
        hole = child - 1;
    }
    PushHeap(first, hole, top, value, less);
}

template <class T, class Less>
void HeapSort(T* first, T* last, Less less) {
    ptrdiff_t len = last - first;
    for (ptrdiff_t parent = (len - 2) / 2;; --parent) {
        AdjustHeap(first, parent, len, first[parent], less);
        if (parent == 0) break;
    }
    while (last - first > 1) {
        --last;
        T value = *last;
        *last = *first;
        AdjustHeap(first, (ptrdiff_t)0, last - first, value, less);
    }
}

template <class T, class Less>
const T& Median(const T& a, const T& b, const T& c, Less less) {
    if (less(a, b)) {
        if (less(b, c)) return b;
        if (less(a, c)) return c;
        return a;
    }
    if (less(a, c)) return a;
    if (less(b, c)) return c;
    return b;
}

template <class T, class Less>
T* UnguardedPartition(T* first, T* last, T pivot, Less less) {
    for (;;) {
        while (less(*first, pivot)) ++first;
        --last;
        while (less(pivot, *last)) --last;
        if (!(first < last)) return first;
        std::swap(*first, *last);
        ++first;
    }
}

template <class T, class Less>
void IntrosortLoop(T* first, T* last, int depth, Less less) {
    while (last - first > 16) {
        if (depth == 0) {
            HeapSort(first, last, less);
            return;
        }
        --depth;
        T pivot = Median(*first, first[(last - first) / 2], *(last - 1), less);
        T* cut = UnguardedPartition(first, last, pivot, less);
        IntrosortLoop(cut, last, depth, less);
        last = cut;
    }
}

template <class T, class Less>
void LinearInsert(T* first, T* last, T value, Less less) {
    if (less(value, *first)) {
        for (T* p = last; p != first; --p) *p = *(p - 1);
        *first = value;
        return;
    }
    T* next = last - 1;
    while (less(value, *next)) {
        *last = *next;
        last = next--;
    }
    *last = value;
}

template <class T, class Less>
void InsertionSort(T* first, T* last, Less less) {
    if (first == last) return;
    for (T* i = first + 1; i != last; ++i) LinearInsert(first, i, *i, less);
}

template <class T, class Less>
void Sort(T* first, T* last, Less less) {
    if (first == last) return;
    ptrdiff_t n = last - first;
    int lg = 0;
    for (ptrdiff_t k = n; k != 1; k >>= 1) ++lg;
    IntrosortLoop(first, last, lg * 2, less);
    if (n > 16) {
        InsertionSort(first, first + 16, less);
        for (T* i = first + 16; i != last; ++i) {   // __unguarded_linear_insert
            T value = *i;
            T* p = i;
            while (less(value, *(p - 1))) {
                *p = *(p - 1);
                --p;
            }
            *p = value;
        }
    } else {
        InsertionSort(first, last, less);
    }
}

}  // namespace StlSort
