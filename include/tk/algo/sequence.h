/**
 * @file sequence.h
 * @brief Implements generic, non-modifying sequence algorithms.
 *
 * @details
 * This file provides generic algorithms that operate on iterator ranges
 * (begin, end) to perform sequence operations, such as searching and counting.
 *
 * All functions in this file are implemented as `static inline` to
 * achieve zero-cost abstraction, allowing the compiler to fully inline
 * the algorithm logic and optimize away the iterator vtable calls
 * where possible.
 *
 * These algorithms are "generic" because they operate entirely on the
 * `tk_iterator_t` interface and have no knowledge of the underlying
 * container (e.g., `tk_vec_t`).
 */
#ifndef TOOLKIT_SEQUENCE_H
#define TOOLKIT_SEQUENCE_H

#include <tk/core/iterator.h>
#include <tk/core/types.h>

/**
 * @brief Finds the first element in the range [begin, end) that satisfies
 * the given predicate.
 *
 * @details
 * This algorithm iterates from `begin` up to (but not including) `end`.
 * For each element, it calls the `predicate` function. The first time
 * `predicate` returns `true`, this function stops and returns the
 * iterator pointing to that element.
 *
 * @param begin The iterator marking the beginning of the range to search.
 * @param end The iterator marking the end of the range (one-past-the-last).
 * @param predicate A function pointer that takes a `const void*` to an
 * element and returns `true` if the element matches,
 * `false` otherwise.
 * @return `tk_iterator_t` pointing to the first matching element. If no
 * element satisfies the predicate, this function returns the `end`
 * iterator.
 */
static inline tk_iterator_t
tk_algo_find_if(tk_iterator_t begin, tk_iterator_t end,
                tk_bool (*predicate)(const void *element)) {

  // Loop while the current iterator 'begin' is not equal to 'end'
  while (!tk_iter_equal(&begin, &end)) {
    // Get the current element from the iterator
    const void *element = tk_iter_get(&begin);

    // Test the element using the user's predicate function
    if (predicate(element)) {
      // Found it. Return the current iterator.
      return begin;
    }

    // Not found. Advance the iterator to the next position
    tk_iter_next(&begin);
  }

  // Reached the end of the range without finding a match.
  //    Return the 'end' iterator.
  return end;
}

#endif // TOOLKIT_SEQUENCE_H
