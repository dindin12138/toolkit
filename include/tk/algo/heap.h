/**
 * @file heap.h
 * @brief Implements the heap family: build, extend, shrink, drain and query a
 * max-heap over an existing range, in place.
 *
 * @details
 * A caller can treat any range with random access as a priority queue without
 * copying it anywhere: build the heap over the range (tk_algo_make_heap), push
 * one element at a time (tk_algo_push_heap), pop the top one at a time
 * (tk_algo_pop_heap) and finally drain the whole range into ascending order
 * (tk_algo_sort_heap). tk_algo_is_heap reports whether a range currently
 * satisfies the property.
 *
 * The range conventions are the STL's, so the mental model transfers without
 * relearning:
 *  - the heap is a MAX-heap under the comparator, which is why draining yields
 *    ASCENDING order;
 *  - tk_algo_push_heap treats the range minus its last element as the existing
 *    heap and sifts that last element up, so the caller appends first;
 *  - tk_algo_pop_heap moves the root to the last position and restores the
 *    property over the shortened range, leaving removal to the caller;
 *  - push and pop are no-ops on ranges shorter than two elements.
 *
 * @section heap_why_algorithms Why algorithms and not a priority-queue adaptor
 * The family is exposed as five algorithms rather than wrapped in a
 * priority-queue adaptor because this is an algorithm library: an adaptor is a
 * container-layer type, and a container would have to own storage the caller
 * may already have. Everything a priority queue does is already expressible by
 * calling these five functions over a range the caller owns, with no allocation
 * and no second copy of the data. A future adaptor, if one is ever wanted,
 * would sit on top of these functions rather than replace them -- so nothing
 * here would have to change.
 *
 * @section heap_release_behaviour WARNING: an unusable range is silently
 * ignored in release builds
 * Every entry point here requires RANDOM_ACCESS iterators. Passing a
 * bidirectional or forward range is a caller violation: debug builds assert,
 * but release builds (NDEBUG) compile the assertion out and the function
 * RETURNS IMMEDIATELY, LEAVING THE RANGE UNTOUCHED, WITH NO ERROR AND NO OTHER
 * SIGNAL. The entry points return `void` (tk_algo_is_heap returns `false`), so
 * there is nothing to check. A NULL comparator is treated the same way: it is
 * a caller violation, and in release the call is skipped rather than crashing.
 * A container that does not advertise random access must not be passed to this
 * family; the mistake cannot be reported at run time in a release build.
 *
 * @section heap_contract General contract
 * - No function here allocates, and none returns an error code.
 * - No function here references a concrete container: they depend only on the
 *   `core` module and the polymorphic iterator interface, plus tk_algo_swap
 *   from <tk/algo/sequence.h>, which is the only byte-swapping implementation
 *   in the codebase.
 * - `cmp` must not be NULL and must implement a strict weak ordering; violating
 *   that is undefined behaviour.
 * - Draining a range that is not a heap produces silent garbage rather than a
 *   crash, which is expensive to diagnose, so tk_algo_sort_heap asserts the
 *   heap property in debug builds before it starts.
 *
 * All functions in this file are `static inline`. This inlines the thin wrapper
 * layer only: the underlying vtable calls are indirect function-pointer calls
 * resolved at run time and cannot be devirtualized.
 */
#ifndef TOOLKIT_ALGO_HEAP_H
#define TOOLKIT_ALGO_HEAP_H

#include <tk/algo/sequence.h> // tk_algo_swap: the only byte-swapping code
#include <tk/core/iterator.h>
#include <tk/core/macros.h>
#include <tk/core/types.h>

/* ---------------------------------------------------------------------------
 * Internal helpers -- NOT part of the public API.
 *
 * Application code must not call anything in this section. These live in the
 * header only because the family is header-only, and because the sort module
 * reuses the sift primitives instead of duplicating them.
 * ------------------------------------------------------------------------- */

/**
 * @brief The release-time verdict of the preconditions shared by every entry
 * point.
 *
 * Kept separate from the assertions below so that exactly one expression
 * decides "may this call proceed", and the assertion checks cannot drift away
 * from it.
 *
 * @return `true` when the range and comparator are usable.
 */
static inline tk_bool tk_heap_range_ok(tk_iterator_t begin, tk_iterator_t end,
                                       tk_comparator_t cmp) {
  return cmp != NULL && begin.vtable != NULL && begin.vtable == end.vtable &&
         begin.vtable->category == TK_ITER_RANDOM_ACCESS;
}

/**
 * @brief Asserts the shared preconditions, then reports the same verdict.
 *
 * This is a macro rather than a function because each assertion message must
 * name the entry point that was called, and `assert` derives its message from
 * the source text -- so the name has to be spliced in at the call site. Folding
 * the assertions and the verdict into one expression is what keeps them from
 * disagreeing: a NULL comparator is checked by both, so it cannot crash a
 * release build while the documentation promises the range is left untouched.
 *
 * Usage: `if (!TK_HEAP_ENTRY_OK("tk_algo_make_heap", begin, end, cmp)) return;`
 *
 * @param NAME A string literal naming the calling entry point.
 * @param BEGIN The first element of the range.
 * @param END One past the last element of the range.
 * @param CMP The three-way comparator.
 */
#define TK_HEAP_ENTRY_OK(NAME, BEGIN, END, CMP)                                \
  (TK_ASSERT((BEGIN).vtable != NULL && (END).vtable != NULL &&                 \
             NAME ": iterators must not have NULL vtables."),                  \
   TK_ASSERT((BEGIN).vtable == (END).vtable &&                                 \
             NAME ": 'begin' and 'end' iterators are from different container " \
             "types."),                                                        \
   TK_ASSERT((CMP) != NULL && NAME ": 'cmp' must not be NULL."),               \
   TK_ASSERT((BEGIN).vtable != NULL &&                                         \
             (BEGIN).vtable->category == TK_ITER_RANDOM_ACCESS &&              \
             NAME ": requires RANDOM_ACCESS iterators. A bidirectional or "    \
             "forward range is a caller violation."),                          \
   tk_heap_range_ok(BEGIN, END, CMP))

/**
 * @brief Returns the number of elements in [begin, end).
 *
 * @pre The range must be usable (tk_heap_range_ok) and random access.
 */
static inline size_t tk_heap_size(tk_iterator_t begin, tk_iterator_t end) {
  return (size_t)tk_iter_distance(&begin, &end);
}

/**
 * @brief Returns the address of the element at `index`.
 *
 * @param begin The first element of the range. Passed by value: the caller's
 * iterator is not moved.
 * @param index The element index, which must be within the range.
 * @return A writable pointer to that element.
 */
static inline void *tk_heap_element_at(tk_iterator_t begin, size_t index) {
  tk_iter_advance_by(&begin, (ptrdiff_t)index);
  return tk_iter_get(&begin);
}

/**
 * @brief Exchanges the elements at two indexes of the range.
 *
 * @param begin The first element of the range. Passed by value.
 * @param i The first index.
 * @param j The second index.
 *
 * @pre Both indexes must be within the range.
 */
static inline void tk_heap_swap_at(tk_iterator_t begin, size_t i, size_t j) {
  if (i == j)
    return; // swapping an element with itself would be a wasted memcpy
  tk_iterator_t a = begin;
  tk_iterator_t b = begin;
  tk_iter_advance_by(&a, (ptrdiff_t)i);
  tk_iter_advance_by(&b, (ptrdiff_t)j);
  tk_algo_swap(a, b); // the single byte-swapping implementation in the library
}

/**
 * @brief Restores the heap property by moving one element toward the root.
 *
 * Repeatedly compares the element at `index` with its parent and swaps them
 * while the parent is ordered below it. Stops at the root or as soon as the
 * parent is no longer below. Costs O(log index) comparisons and swaps.
 *
 * @param begin The first element of the range. Passed by value.
 * @param index The index of the element to sift up. Everything before it is
 * assumed to satisfy the heap property already.
 * @param cmp The three-way comparator.
 *
 * @pre `index` must be within the range.
 */
static inline void tk_heap_sift_up(tk_iterator_t begin, size_t index,
                                   tk_comparator_t cmp) {
  while (index > 0) {
    const size_t parent = (index - 1) / 2;
    if (cmp(tk_heap_element_at(begin, parent),
            tk_heap_element_at(begin, index)) >= 0)
      return; // the parent is not below the child: the property holds
    tk_heap_swap_at(begin, parent, index);
    index = parent;
  }
}

/**
 * @brief Restores the heap property by moving one element toward the leaves.
 *
 * Repeatedly follows the greater child while that child is ordered above the
 * element being sifted. Costs O(log n) comparisons and swaps.
 *
 * @param begin The first element of the range. Passed by value.
 * @param root The index of the element to sift down.
 * @param n The number of elements the heap covers, counted from `begin`. The
 * subtree below `root` must already satisfy the heap property.
 * @param cmp The three-way comparator.
 *
 * @pre `root < n`.
 */
static inline void tk_heap_sift_down(tk_iterator_t begin, size_t root,
                                     size_t n, tk_comparator_t cmp) {
  for (;;) {
    // A node has children exactly when root < n / 2. Testing that first keeps
    // 2 * root + 1 from overflowing, whatever n is.
    if (root >= n / 2)
      return; // leaf: nothing below to sift past
    const size_t left = 2 * root + 1;

    size_t greater = left;
    const size_t right = left + 1;
    if (right < n && cmp(tk_heap_element_at(begin, left),
                         tk_heap_element_at(begin, right)) < 0)
      greater = right; // the right child is ordered above the left one

    if (cmp(tk_heap_element_at(begin, root),
            tk_heap_element_at(begin, greater)) >= 0)
      return; // the element is not below its greater child: stop

    tk_heap_swap_at(begin, root, greater);
    root = greater;
  }
}

/**
 * @brief Reports whether the first `n` elements of the range satisfy the heap
 * property.
 *
 * @param begin The first element of the range. Passed by value.
 * @param n The number of elements the heap covers, counted from `begin`.
 * @param cmp The three-way comparator.
 * @return `true` if no element is ordered above its parent.
 */
static inline tk_bool tk_heap_is_heap_range(tk_iterator_t begin, size_t n,
                                            tk_comparator_t cmp) {
  for (size_t i = 1; i < n; ++i) {
    const size_t parent = (i - 1) / 2;
    if (cmp(tk_heap_element_at(begin, parent), tk_heap_element_at(begin, i)) <
        0)
      return false;
  }
  return true;
}

/* ---------------------------------------------------------------------------
 * Public API.
 * ------------------------------------------------------------------------- */

/**
 * @brief Rearranges [begin, end) into a max-heap, in place.
 *
 * @details
 * Sifts down every element that has children, from the last such element back
 * to the root. That is linear time, not O(n log n): the elements near the
 * leaves -- the majority -- barely move.
 *
 * The range is not required to be a heap on entry; it may be in any order. On
 * exit the root is the maximum under `cmp`. Ranges of fewer than two elements
 * are already heaps and are left alone.
 *
 * @param begin The first element of the range.
 * @param end One past the last element of the range.
 * @param cmp The three-way comparator.
 *
 * @pre Random-access iterators, a valid range and a non-NULL `cmp`. A violation
 * asserts in debug builds; in release builds the function returns immediately
 * and leaves the range untouched (see the warning in the file header).
 */
static inline void tk_algo_make_heap(tk_iterator_t begin, tk_iterator_t end,
                                     tk_comparator_t cmp) {
  if (!TK_HEAP_ENTRY_OK("tk_algo_make_heap", begin, end, cmp))
    return;

  const size_t n = tk_heap_size(begin, end);
  if (n < 2)
    return;

  for (size_t i = n / 2; i-- > 0;)
    tk_heap_sift_down(begin, i, n, cmp);
}

/**
 * @brief Extends an existing heap by one element, in place.
 *
 * @details
 * The range minus its last element must already be a heap; the last element is
 * the one being pushed. The caller appends the new element to the range first
 * (tk_vec_push_back, or writing past the old end), then calls this function,
 * which sifts it up into place. Costs O(log n) comparisons and swaps.
 *
 * Following the STL convention, nothing is removed and no storage is touched:
 * the element is already in the range, this only restores the property.
 *
 * Ranges of fewer than two elements are no-ops: a single element is already a
 * heap and there is no parent to sift past.
 *
 * @param begin The first element of the range.
 * @param end One past the last element of the range; the pushed element is the
 * one before it.
 * @param cmp The three-way comparator.
 *
 * @pre Random-access iterators, a valid range and a non-NULL `cmp`. A violation
 * asserts in debug builds; in release builds the function returns immediately
 * and leaves the range untouched (see the warning in the file header).
 * @pre The range minus its last element must already be a heap. That is not
 * checked, even in debug builds: it would turn an O(log n) operation into an
 * O(n) one.
 */
static inline void tk_algo_push_heap(tk_iterator_t begin, tk_iterator_t end,
                                     tk_comparator_t cmp) {
  if (!TK_HEAP_ENTRY_OK("tk_algo_push_heap", begin, end, cmp))
    return;

  const size_t n = tk_heap_size(begin, end);
  if (n < 2)
    return;

  tk_heap_sift_up(begin, n - 1, cmp);
}

/**
 * @brief Removes the maximum element from a heap, in place.
 *
 * @details
 * Swaps the root with the last element, then sifts the new root down over the
 * shortened range. On exit the maximum is at the last position and everything
 * before it is a heap again. Costs O(log n) comparisons and swaps.
 *
 * Following the STL convention, nothing is removed and no storage is touched:
 * the range keeps its size, and removing the last element is the caller's job
 * (tk_vec_pop_back). That is what makes the operation allocation-free: the
 * popped element reuses the slot it was already in.
 *
 * Ranges of fewer than two elements are no-ops: there is no second element to
 * swap with and nothing left to sift.
 *
 * @param begin The first element of the range.
 * @param end One past the last element of the range.
 * @param cmp The three-way comparator.
 *
 * @pre Random-access iterators, a valid range and a non-NULL `cmp`. A violation
 * asserts in debug builds; in release builds the function returns immediately
 * and leaves the range untouched (see the warning in the file header).
 * @pre The range must already be a heap; tk_algo_pop_heap does not check.
 */
static inline void tk_algo_pop_heap(tk_iterator_t begin, tk_iterator_t end,
                                    tk_comparator_t cmp) {
  if (!TK_HEAP_ENTRY_OK("tk_algo_pop_heap", begin, end, cmp))
    return;

  const size_t n = tk_heap_size(begin, end);
  if (n < 2)
    return;

  tk_heap_swap_at(begin, 0, n - 1); // the maximum moves out of the way
  tk_heap_sift_down(begin, 0, n - 1, cmp);
}

/**
 * @brief Drains a heap into ascending order, in place.
 *
 * @details
 * Repeatedly pops the maximum to the end of the still-heap part of the range.
 * Because the popped maxima pile up from the back, the finished range reads
 * ascending under `cmp`. Costs O(n log n) comparisons and swaps and allocates
 * nothing; this is heapsort, reachable through the same range conventions as
 * the rest of the family rather than a separate entry point.
 *
 * Ranges of fewer than two elements are no-ops.
 *
 * @param begin The first element of the range.
 * @param end One past the last element of the range.
 * @param cmp The three-way comparator.
 *
 * @pre Random-access iterators, a valid range and a non-NULL `cmp`. A violation
 * asserts in debug builds; in release builds the function returns immediately
 * and leaves the range untouched (see the warning in the file header).
 * @pre The range must be a heap. Draining a non-heap does not crash: it yields
 * silent garbage. Because that is expensive to diagnose, debug builds assert
 * the heap property before starting; release builds skip the check, which
 * costs O(n) and would turn draining into O(n) extra work.
 */
static inline void tk_algo_sort_heap(tk_iterator_t begin, tk_iterator_t end,
                                     tk_comparator_t cmp) {
  if (!TK_HEAP_ENTRY_OK("tk_algo_sort_heap", begin, end, cmp))
    return;

  const size_t n = tk_heap_size(begin, end);
  if (n < 2)
    return;

  TK_ASSERT(tk_heap_is_heap_range(begin, n, cmp) &&
            "tk_algo_sort_heap: the range is not a heap. Draining it would "
            "produce silent garbage; call tk_algo_make_heap first.");

  // Pop into a shrinking prefix: `last` retreats one position per iteration,
  // so the maxima accumulate at the back of the range.
  tk_iterator_t last = end;
  for (size_t remaining = n; remaining > 1; --remaining) {
    tk_algo_pop_heap(begin, last, cmp);
    tk_iter_prev(&last);
  }
}

/**
 * @brief Reports whether [begin, end) satisfies the heap property.
 *
 * @details
 * Checks every non-root element against its parent, so the answer is exact and
 * costs O(n) comparisons. Zero- and one-element ranges are trivially heaps.
 *
 * This is the diagnostic companion to the rest of the family: a corrupted heap
 * produces a wrong answer from tk_algo_sort_heap rather than a crash, so
 * "is this range still a heap?" is the cheapest way to find out why.
 *
 * @param begin The first element of the range.
 * @param end One past the last element of the range.
 * @param cmp The three-way comparator; the heap is a max-heap under it.
 * @return `true` if no element is ordered above its parent.
 *
 * @pre Random-access iterators, a valid range and a non-NULL `cmp`. A violation
 * asserts in debug builds; in release builds the function returns `false` and
 * leaves the range untouched (see the warning in the file header).
 */
static inline tk_bool tk_algo_is_heap(tk_iterator_t begin, tk_iterator_t end,
                                      tk_comparator_t cmp) {
  if (!TK_HEAP_ENTRY_OK("tk_algo_is_heap", begin, end, cmp))
    return false;

  return tk_heap_is_heap_range(begin, tk_heap_size(begin, end), cmp);
}

#endif // TOOLKIT_ALGO_HEAP_H
