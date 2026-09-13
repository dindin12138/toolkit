/**
 * @file numeric.h
 * @brief Implements generic numeric algorithms that operate on iterator
 * ranges.
 *
 * @details
 * This header holds algorithms whose mental model matches the C++ STL
 * `<numeric>` header (as opposed to the sequence operations in sequence.h).
 * The first such algorithm is tk_algo_accumulate; future numeric algorithms
 * (reduce, inner_product, partial_sum, ...) belong here as well, which keeps
 * sequence.h from growing without bound.
 *
 * Like sequence.h, every function here is `static inline` and depends ONLY on
 * the `core` module (it never includes a `ds` header). The `static inline`
 * keyword removes the thin wrapper overhead but does not devirtualize the
 * underlying vtable calls.
 */
#ifndef TOOLKIT_ALGO_NUMERIC_H
#define TOOLKIT_ALGO_NUMERIC_H

#include <tk/core/iterator.h>
#include <tk/core/macros.h>
#include <tk/core/types.h>

/**
 * @brief Folds the range [begin, end) into a caller-owned accumulator.
 *
 * @details
 * The caller holds and initializes `*acc` (its "initial value"). For each
 * element the algorithm calls `op(acc, element)`, which updates the
 * accumulator in place. Because the result already lives in the caller's
 * storage, the function returns `void` (returning `acc` again would be
 * redundant). This mirrors the STL's `std::accumulate` "init as first
 * argument" idea, adapted to C.
 *
 * @param begin The iterator marking the beginning of the range.
 * @param end The iterator marking the end of the range (one-past-the-last).
 * @param acc A pointer to the caller's accumulator, pre-initialized to the
 * desired initial value.
 * @param op A function pointer that reads an element and updates `*acc`.
 *
 * @pre `acc` and `op` must not be NULL. Passing NULL is a caller violation:
 * debug builds assert, release builds (NDEBUG) are undefined behaviour
 * (typically a crash); a NULL `op` is harmless only when the range is empty.
 * @note On an empty range `op` is never called and `*acc` is left unchanged.
 */
static inline void
tk_algo_accumulate(tk_iterator_t begin, tk_iterator_t end, void *acc,
                   void (*op)(void *acc, const void *element)) {
  TK_ASSERT(begin.vtable != NULL && end.vtable != NULL &&
            "tk_algo_accumulate: Iterators must not have NULL vtables.");
  TK_ASSERT(begin.vtable == end.vtable &&
            "tk_algo_accumulate: 'begin' and 'end' iterators are from "
            "different container types.");
  TK_ASSERT(acc != NULL && "tk_algo_accumulate: 'acc' must not be NULL.");
  TK_ASSERT(op != NULL && "tk_algo_accumulate: 'op' must not be NULL.");

  while (!tk_iter_equal(&begin, &end)) {
    op(acc, tk_iter_get(&begin));
    tk_iter_next(&begin);
  }
}

#endif // TOOLKIT_ALGO_NUMERIC_H
