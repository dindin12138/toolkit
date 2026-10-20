/**
 * @file sort.h
 * @brief Implements tk_algo_sort: an in-place introsort over any range with
 * random access.
 *
 * @details
 * tk_algo_sort orders a range in place. It takes no size argument, allocates
 * nothing, and has no failure mode: the element size is read from the iterator
 * protocol, so the same call sorts ints, twelve-byte structs and anything else
 * a random-access iterator can describe.
 *
 * The strategy is introsort (ADR-0002), which is three algorithms in one:
 *  - it starts as quicksort, partitioning around a median-of-three pivot;
 *  - it switches to heapsort for any sub-range whose recursion has already
 *    grown past twice log2(n) levels, which bounds the worst case at
 *    O(n log n) on adversarial input and keeps the recursion depth
 *    logarithmic no matter what the input looks like;
 *  - it finishes with one insertion-sort pass, which is the cheapest way to
 *    clean up the short, nearly ordered sub-ranges quicksort leaves behind.
 *
 * The partition is three-way: elements below the pivot, elements equal to it
 * and elements above it are separated in a single pass, and only the outer two
 * regions are sorted further. A range of equal elements therefore finishes in
 * linear time instead of degrading, which is what makes the three-way
 * comparator (ADR-0003) pay for itself.
 *
 * @section sort_not_stable WARNING: the sort is NOT stable
 * Equal elements may end up in any order relative to each other. This is
 * deliberate, not an oversight: a stable sort needs O(n) scratch space, and
 * this sort allocates nothing (ADR-0002). If equal elements must keep their
 * relative order, this function is the wrong tool -- no flag, no variant of it
 * will do that. A stable sort would have to take its scratch space from the
 * caller, and no such function exists yet.
 *
 * @section sort_release_behaviour WARNING: an unusable range is silently
 * ignored in release builds
 * This function requires RANDOM_ACCESS iterators. Passing a bidirectional or
 * forward range -- a tk_list_t, for example -- is a caller violation: debug
 * builds assert, but release builds (NDEBUG) compile the assertion out and the
 * function RETURNS IMMEDIATELY, LEAVING THE RANGE UNTOUCHED, WITH NO ERROR AND
 * NO OTHER SIGNAL. The function returns `void`, so there is nothing to check.
 * A NULL comparator is treated the same way. The mistake is therefore
 * indistinguishable from a successful sort of an already ordered range, which
 * is why the requirement is stated here rather than only at the parameter.
 *
 * @section sort_contract General contract
 * - No allocation: the whole algorithm runs in the range it was given, using
 *   only iterator movement and byte-wise swaps.
 * - Worst case O(n log n) comparisons and swaps, for every input.
 * - The median-of-three pivot keeps already sorted and reverse-sorted input on
 *   the balanced path instead of the degenerate one; a range whose elements all
 *   compare equal costs a single partition pass, so duplicates are linear.
 * - Recursion depth is O(log n) for every input, so a large range cannot
 *   overflow the stack.
 * - `cmp` must not be NULL and must implement a strict weak ordering; violating
 *   that is undefined behaviour.
 * - The range is reordered but never resized: sorting is a permutation of the
 *   elements that were there.
 *
 * All functions in this file are `static inline`. This inlines the thin wrapper
 * layer only: the underlying vtable calls are indirect function-pointer calls
 * resolved at run time and cannot be devirtualized.
 */
#ifndef TOOLKIT_ALGO_SORT_H
#define TOOLKIT_ALGO_SORT_H

// The entry-point guard and the range helpers (element_at, swap_at, size) come
// from the heap family, which requires exactly the same range contract; the
// heapsort fallback reuses its sift down instead of carrying a second copy.
#include <tk/algo/heap.h>
#include <tk/core/iterator.h>
#include <tk/core/macros.h>
#include <tk/core/types.h>

/* ---------------------------------------------------------------------------
 * Internal helpers -- NOT part of the public API.
 *
 * Application code must not call anything in this section, and must not rely on
 * the value of the threshold below. They live in the header only because the
 * family is header-only.
 *
 * They build on the heap module's range helpers (tk_heap_element_at,
 * tk_heap_swap_at, tk_heap_size) and on its entry-point guard
 * (TK_HEAP_ENTRY_OK) rather than re-deriving them: sort requires exactly the
 * same range contract, and the heapsort fallback calls the heap family's own
 * sift down instead of carrying a second copy of it.
 * ------------------------------------------------------------------------- */

/**
 * @brief Sub-ranges at or below this many elements are finished by insertion
 * sort instead of being partitioned further.
 *
 * Quicksort's recursion is not worth its overhead on short ranges, and
 * insertion sort is close to optimal on the nearly ordered runs it is handed.
 * 16 is the usual choice: large enough that the recursion stops early, small
 * enough that the quadratic tail stays negligible.
 */
#define TK_SORT_INSERTION_THRESHOLD 16

/**
 * @brief The two boundaries a three-way partition establishes.
 *
 * [begin, equal_begin) holds the elements ordered below the pivot,
 * [equal_begin, equal_end) holds the elements ordered equal to it, and
 * [equal_end, n) holds the elements ordered above it. Only the first and last
 * regions still need sorting.
 */
typedef struct {
  size_t equal_begin;
  size_t equal_end;
} tk_sort_partition_t;

/**
 * @brief Returns the recursion budget for a range of `n` elements: twice
 * floor(log2(n)).
 *
 * Twice log2(n) is the classic introsort bound. It leaves the median-of-three
 * pivot room to be unlucky -- a perfectly balanced partition would need only
 * log2(n) levels -- while still refusing to recurse anywhere near n levels,
 * which is what an adversarial input would otherwise demand.
 *
 * @param n The number of elements in the range. Must be at least 1.
 * @return The number of partitions allowed before the heapsort fallback.
 */
static inline unsigned tk_sort_depth_limit(size_t n) {
  unsigned levels = 0;
  while (n > 1) {
    n >>= 1;
    ++levels;
  }
  return 2u * levels;
}

/**
 * @brief Returns the index of the median of the first, middle and last
 * elements of the range.
 *
 * The median of three is the pivot choice that makes quicksort robust against
 * already sorted and reverse-sorted input: on either of those the median is the
 * true middle element, so the partition is balanced instead of degenerate. It
 * costs two or three comparisons, which the partition would have spent anyway.
 *
 * @param begin The first element of the range. Passed by value.
 * @param n The number of elements in the range. Must be at least 2 (a shorter
 * range is never partitioned).
 * @param cmp The three-way comparator.
 * @return The index of the median element: 0, n / 2 or n - 1.
 */
static inline size_t tk_sort_median_of_three(tk_iterator_t begin, size_t n,
                                             tk_comparator_t cmp) {
  const size_t mid = n / 2;
  const size_t last = n - 1;

  if (cmp(tk_heap_element_at(begin, 0), tk_heap_element_at(begin, mid)) > 0) {
    // first > mid
    if (cmp(tk_heap_element_at(begin, mid), tk_heap_element_at(begin, last)) >
        0)
      return mid; // last < mid < first
    // mid <= last: the median is the smaller of first and last
    return cmp(tk_heap_element_at(begin, 0), tk_heap_element_at(begin, last)) >
                   0
               ? last
               : 0;
  }
  // first <= mid
  if (cmp(tk_heap_element_at(begin, 0), tk_heap_element_at(begin, last)) > 0)
    return 0; // last < first <= mid
  // first <= last: the median is the smaller of mid and last
  return cmp(tk_heap_element_at(begin, mid), tk_heap_element_at(begin, last)) >
                 0
             ? last
             : mid;
}

/**
 * @brief Partitions the range three ways around its median-of-three pivot.
 *
 * The pivot is first swapped to index 0 and stays there, untouched, for the
 * whole scan: every element is compared against it once, and is then pushed
 * into the below region (swapped to the front of the unscanned part) or the
 * above region (swapped to the back of it). Elements that compare equal to the
 * pivot are left where they are, so the unscanned part shrinks from both sides
 * and duplicates cost one comparison each rather than a second pass.
 *
 * At the end the pivot is swapped into the gap between the two outer regions,
 * which extends the equal region to include it -- the pivot is equal to
 * itself, so it belongs there rather than needing a region of its own.
 *
 * @param begin The first element of the range. Passed by value.
 * @param n The number of elements in the range. Must be at least 2.
 * @param cmp The three-way comparator.
 * @return The boundaries of the equal region (see tk_sort_partition_t).
 */
static inline tk_sort_partition_t tk_sort_partition(tk_iterator_t begin,
                                                    size_t n,
                                                    tk_comparator_t cmp) {
  const size_t pivot_index = tk_sort_median_of_three(begin, n, cmp);
  tk_heap_swap_at(begin, 0, pivot_index); // the pivot parks at index 0

  size_t less = 1;    // next free slot of the below region
  size_t scan = 1;    // next element to classify
  size_t greater = n; // first slot of the above region

  while (scan < greater) {
    const int order = cmp(tk_heap_element_at(begin, scan),
                          tk_heap_element_at(begin, 0));
    if (order < 0) {
      tk_heap_swap_at(begin, less, scan);
      ++less;
      ++scan;
    } else if (order > 0) {
      --greater;
      tk_heap_swap_at(begin, scan, greater);
      // `scan` now holds an unclassified element: do not advance it
    } else {
      ++scan; // equal to the pivot: it stays where it is
    }
  }

  // Move the pivot between the below and above regions. Its old slot at index 0
  // is filled by the last element of the below region, which belongs there.
  tk_heap_swap_at(begin, 0, less - 1);

  tk_sort_partition_t bounds;
  bounds.equal_begin = less - 1;
  bounds.equal_end = greater;
  return bounds;
}

/**
 * @brief Sorts the range with insertion sort, in place.
 *
 * Walks the range once, inserting each element into the sorted prefix behind
 * it by swapping it left past every element it is ordered below. Linear on an
 * already ordered range, quadratic on a reversed one -- which is why it is only
 * ever handed short ranges or the nearly ordered leftovers of the partitions.
 *
 * @param begin The first element of the range. Passed by value.
 * @param n The number of elements in the range.
 * @param cmp The three-way comparator.
 */
static inline void tk_sort_insertion_sort(tk_iterator_t begin, size_t n,
                                          tk_comparator_t cmp) {
  for (size_t i = 1; i < n; ++i) {
    size_t j = i;
    while (j > 0 && cmp(tk_heap_element_at(begin, j - 1),
                        tk_heap_element_at(begin, j)) > 0) {
      tk_heap_swap_at(begin, j - 1, j);
      --j;
    }
  }
}

/**
 * @brief Sorts the first `n` elements of the range with heapsort.
 *
 * The introsort fallback, used when the recursion budget is exhausted: build a
 * heap over the sub-range and drain it. This is where the sort reuses the heap
 * family rather than duplicating its sift down -- the guarantee that the worst
 * case is O(n log n) is bought entirely by code that already existed.
 *
 * @param begin The first element of the range. Passed by value.
 * @param n The number of elements in the range.
 * @param cmp The three-way comparator.
 */
static inline void tk_sort_heapsort(tk_iterator_t begin, size_t n,
                                    tk_comparator_t cmp) {
  tk_iterator_t end = begin;
  tk_iter_advance_by(&end, (ptrdiff_t)n);

  tk_algo_make_heap(begin, end, cmp);
  tk_algo_sort_heap(begin, end, cmp);
}

/**
 * @brief The introsort recursion: partitions, falls back to heapsort when the
 * budget runs out, and stops partitioning below the insertion-sort threshold.
 *
 * The loop always recurses into the smaller of the two outer regions and
 * continues iteratively with the larger one. That is what keeps the stack
 * depth logarithmic even before the budget is consulted: the smaller region is
 * at most half the range, so the number of live frames cannot exceed log2(n).
 * The budget is a second, independent bound, aimed at the total amount of work
 * rather than at the depth.
 *
 * @param begin The first element of the sub-range. Passed by value.
 * @param n The number of elements in the sub-range.
 * @param depth The number of partitions still allowed on this path.
 * @param cmp The three-way comparator.
 */
static inline void tk_sort_introsort_loop(tk_iterator_t begin, size_t n,
                                          unsigned depth,
                                          tk_comparator_t cmp) {
  while (n > TK_SORT_INSERTION_THRESHOLD) {
    if (depth == 0) {
      tk_sort_heapsort(begin, n, cmp); // adversarial input: give up on quicksort
      return;
    }
    --depth;

    const tk_sort_partition_t bounds = tk_sort_partition(begin, n, cmp);
    const size_t below_n = bounds.equal_begin;
    const size_t above_begin = bounds.equal_end;
    const size_t above_n = n - above_begin;

    tk_iterator_t above = begin;
    tk_iter_advance_by(&above, (ptrdiff_t)above_begin);

    // The equal region needs no work at all: every element in it is already in
    // its final place relative to the rest.
    if (below_n < above_n) {
      tk_sort_introsort_loop(begin, below_n, depth, cmp);
      begin = above; // the larger region: iterate instead of recursing
      n = above_n;
    } else {
      tk_sort_introsort_loop(above, above_n, depth, cmp);
      n = below_n;
    }
  }
}

/* ---------------------------------------------------------------------------
 * Public API.
 * ------------------------------------------------------------------------- */

/**
 * @brief Sorts [begin, end) in place, into the order `cmp` dictates.
 *
 * @details
 * Introsort: quicksort with a median-of-three pivot and a three-way partition,
 * heapsort once the recursion budget is spent, insertion sort to finish. See
 * the file header for the strategy, the guarantees and the two warnings --
 * the sort is NOT stable, and a range that does not offer random access is
 * silently left untouched in release builds.
 *
 * @param begin The first element of the range.
 * @param end One past the last element of the range.
 * @param cmp The three-way comparator; the result is ascending under it, so a
 * descending order is a reversed comparator.
 *
 * @pre Random-access iterators, a valid range and a non-NULL `cmp`. A violation
 * asserts in debug builds; in release builds the function returns immediately
 * and leaves the range untouched (see the warning in the file header).
 * @pre `cmp` must implement a strict weak ordering; violating that is undefined
 * behaviour.
 * @post The range holds the same elements in the order `cmp` dictates. The
 * order of elements that compare equal is unspecified (the sort is not stable).
 */
static inline void tk_algo_sort(tk_iterator_t begin, tk_iterator_t end,
                                tk_comparator_t cmp) {
  // The same guard as the heap family: sort needs exactly the same range
  // contract, so the precondition expression is shared rather than duplicated.
  if (!TK_HEAP_ENTRY_OK("tk_algo_sort", begin, end, cmp))
    return;

  const size_t n = tk_heap_size(begin, end);
  if (n < 2)
    return; // nothing to order

  tk_sort_introsort_loop(begin, n, tk_sort_depth_limit(n), cmp);
  // The partitions stopped at the threshold, so short unsorted runs are still
  // scattered through the range. One insertion-sort pass over the whole range
  // merges them: it is linear when every run is already in place relative to
  // its neighbour, which is the state quicksort leaves behind.
  tk_sort_insertion_sort(begin, n, cmp);
}

#endif // TOOLKIT_ALGO_SORT_H
