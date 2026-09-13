/**
 * @file sequence.h
 * @brief Implements generic sequence algorithms (non-modifying and modifying).
 *
 * @details
 * This file provides generic algorithms that operate on iterator ranges
 * (begin, end) to perform sequence operations. It covers:
 *  - non-modifying algorithms: tk_algo_find_if, tk_algo_count,
 *    tk_algo_count_if (they only read elements); and
 *  - modifying algorithms: tk_algo_for_each (in-place mutation of the source
 *    range) and tk_algo_copy / tk_algo_transform (they write into a separate
 *    output range).
 *
 * All functions in this file are `static inline`. Note this only inlines the
 * thin wrapper layer (e.g. tk_iter_get); the underlying vtable calls are
 * indirect function-pointer calls resolved at run time and cannot be
 * devirtualized/inlined by the compiler. `static inline` therefore reduces
 * wrapper overhead but does NOT make these algorithms "zero-cost".
 *
 * These algorithms are "generic" because they operate entirely on the
 * `tk_iterator_t` interface and have no knowledge of the underlying
 * container (e.g., `tk_vec_t`). This file depends ONLY on the `core` module
 * and never includes a `ds` header.
 */
#ifndef TOOLKIT_ALGO_SEQUENCE_H
#define TOOLKIT_ALGO_SEQUENCE_H

#include <string.h> // For memcpy (used by tk_algo_copy)

#include <tk/core/iterator.h>
#include <tk/core/macros.h>
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

  // Debug-mode type safety check.
  // Asserts that the begin and end iterators are not NULL and are of the
  // same type.
  TK_ASSERT(begin.vtable != NULL && end.vtable != NULL &&
            "tk_algo_find_if: Iterators must not have NULL vtables.");
  TK_ASSERT(begin.vtable == end.vtable &&
            "tk_algo_find_if: 'begin' and 'end' iterators are from "
            "different container types.");

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

/**
 * @brief Applies a function to every element in the range [begin, end).
 *
 * @details
 * For each element the algorithm obtains a *writable* pointer to it (via the
 * iterator's `get`) and calls `f` with it, so `f` may mutate the element in
 * place. Because C has no stateful functor objects, the function returns
 * `void` (the STL convention of returning the function object is dropped).
 *
 * @param begin The iterator marking the beginning of the range.
 * @param end The iterator marking the end of the range (one-past-the-last).
 * @param f A function pointer called once per element with a writable pointer
 * to that element.
 *
 * @pre `f` must not be NULL. Passing NULL is a caller violation: debug builds
 * assert, release builds (NDEBUG) are undefined behaviour (typically a crash).
 * @note On an empty range `f` is never called (so a NULL `f` is harmless only
 * when the range is empty).
 */
static inline void tk_algo_for_each(tk_iterator_t begin, tk_iterator_t end,
                                    void (*f)(void *element)) {
  TK_ASSERT(begin.vtable != NULL && end.vtable != NULL &&
            "tk_algo_for_each: Iterators must not have NULL vtables.");
  TK_ASSERT(begin.vtable == end.vtable &&
            "tk_algo_for_each: 'begin' and 'end' iterators are from "
            "different container types.");
  TK_ASSERT(f != NULL && "tk_algo_for_each: 'f' must not be NULL.");

  while (!tk_iter_equal(&begin, &end)) {
    f(tk_iter_get(&begin)); // f receives a writable pointer
    tk_iter_next(&begin);
  }
}

/**
 * @brief Counts the elements in [begin, end) that satisfy a predicate.
 *
 * @param begin The iterator marking the beginning of the range.
 * @param end The iterator marking the end of the range (one-past-the-last).
 * @param predicate A function pointer returning `true` for a matching element.
 * @return The number of elements for which `predicate` returned `true`
 * (0 for an empty range).
 *
 * @pre `predicate` must not be NULL. Passing NULL is a caller violation: debug
 * builds assert, release builds (NDEBUG) are undefined behaviour (typically a
 * crash); it is harmless only when the range is empty.
 */
static inline size_t tk_algo_count_if(tk_iterator_t begin, tk_iterator_t end,
                                      tk_bool (*predicate)(const void *element)) {
  TK_ASSERT(begin.vtable != NULL && end.vtable != NULL &&
            "tk_algo_count_if: Iterators must not have NULL vtables.");
  TK_ASSERT(begin.vtable == end.vtable &&
            "tk_algo_count_if: 'begin' and 'end' iterators are from "
            "different container types.");
  TK_ASSERT(predicate != NULL && "tk_algo_count_if: 'predicate' must not be "
                                 "NULL.");

  size_t count = 0;
  while (!tk_iter_equal(&begin, &end)) {
    if (predicate(tk_iter_get(&begin))) {
      ++count;
    }
    tk_iter_next(&begin);
  }
  return count;
}

/**
 * @brief Counts the elements in [begin, end) that compare equal to a value.
 *
 * @details
 * Equality is decided by the caller-supplied `eq(element, value)` predicate,
 * because the algorithm cannot know the concrete element type under type
 * erasure. This coexists with tk_algo_count_if: without closures in C, a
 * value-plus-comparator `count` and a predicate-based `count_if` are two
 * genuinely different call forms.
 *
 * @param begin The iterator marking the beginning of the range.
 * @param end The iterator marking the end of the range (one-past-the-last).
 * @param value A pointer to the value to compare against.
 * @param eq A function pointer returning `true` when `element` equals
 * `value`.
 * @return The number of matching elements (0 for an empty range).
 *
 * @pre `eq` must not be NULL. Passing NULL is a caller violation: debug builds
 * assert, release builds (NDEBUG) are undefined behaviour (typically a crash);
 * it is harmless only when the range is empty.
 */
static inline size_t tk_algo_count(tk_iterator_t begin, tk_iterator_t end,
                                   const void *value,
                                   tk_bool (*eq)(const void *element,
                                                 const void *value)) {
  TK_ASSERT(begin.vtable != NULL && end.vtable != NULL &&
            "tk_algo_count: Iterators must not have NULL vtables.");
  TK_ASSERT(begin.vtable == end.vtable &&
            "tk_algo_count: 'begin' and 'end' iterators are from "
            "different container types.");
  TK_ASSERT(eq != NULL && "tk_algo_count: 'eq' must not be NULL.");

  size_t count = 0;
  while (!tk_iter_equal(&begin, &end)) {
    if (eq(tk_iter_get(&begin), value)) {
      ++count;
    }
    tk_iter_next(&begin);
  }
  return count;
}

/**
 * @brief Copies the elements of [begin, end) into the output range at `out`.
 *
 * @details
 * Each element is copied with `memcpy(dst, src, element_size)`. Under type
 * erasure the algorithm cannot know the element size (the vtable does not
 * expose it), so the caller -- who always knows it -- must pass `element_size`
 * explicitly. This is a deliberate deviation from STL's three-argument
 * `copy(first, last, out)`, which needs no size because the type is known at
 * compile time.
 *
 * @param begin The iterator marking the beginning of the source range.
 * @param end The iterator marking the end of the source range.
 * @param out The iterator marking the start of the output range. Must be a
 * valid iterator (vtable != NULL).
 * @param element_size The size in bytes of one element. Must be > 0.
 * @return An iterator positioned one past the last element written.
 *
 * @pre The output range must have room for at least as many elements as the
 * source range. This cannot be detected (iterators carry no upper bound); a
 * too-small output range is undefined behaviour.
 * @pre `out` must be a valid iterator (vtable != NULL). Passing an invalid
 * iterator is a caller violation: debug builds assert, release builds (NDEBUG)
 * compile the assertion out and dereferencing the resulting NULL element
 * pointer is undefined behaviour (typically a crash).
 * @warning The source and output ranges must NOT overlap; overlapping is
 * undefined behaviour. Even the exact self-copy case `out == begin` only
 * happens to work under this implementation -- do NOT rely on it. Use
 * tk_algo_transform / tk_algo_for_each for in-place work.
 */
static inline tk_iterator_t tk_algo_copy(tk_iterator_t begin, tk_iterator_t end,
                                         tk_iterator_t out,
                                         size_t element_size) {
  TK_ASSERT(begin.vtable != NULL && end.vtable != NULL &&
            "tk_algo_copy: Source iterators must not have NULL vtables.");
  TK_ASSERT(begin.vtable == end.vtable &&
            "tk_algo_copy: 'begin' and 'end' iterators are from "
            "different container types.");
  TK_ASSERT(out.vtable != NULL &&
            "tk_algo_copy: 'out' iterator must not have a NULL vtable.");
  TK_ASSERT(element_size > 0 &&
            "tk_algo_copy: 'element_size' must be greater than 0.");

  while (!tk_iter_equal(&begin, &end)) {
    void *src = tk_iter_get(&begin);
    void *dst = tk_iter_get(&out);
    memcpy(dst, src, element_size);
    tk_iter_next(&begin);
    tk_iter_next(&out);
  }
  return out; // one past the last element written
}

/**
 * @brief Transforms the elements of [begin, end) into the output range at
 * `out`.
 *
 * @details
 * For each input element the algorithm obtains a writable pointer to the
 * corresponding output slot and calls `op(out_element, in_element)`; `op` is
 * responsible for any type conversion and for writing the result. Because `op`
 * writes through a `void*`, no `element_size` is needed (unlike tk_algo_copy).
 *
 * @param begin The iterator marking the beginning of the source range.
 * @param end The iterator marking the end of the source range.
 * @param out The iterator marking the start of the output range. Must be a
 * valid iterator (vtable != NULL).
 * @param op A function pointer that reads `in_element` and writes the result
 * through `out_element`.
 * @return An iterator positioned one past the last element written.
 *
 * @pre The output range must have room for at least as many elements as the
 * source range (undetectable; a too-small range is undefined behaviour).
 * @pre `out` must be a valid iterator (vtable != NULL); passing an invalid
 * iterator is a caller violation (debug assertion; release: undefined
 * behaviour).
 * @pre `op` must not be NULL. Passing NULL is a caller violation: debug builds
 * assert, release builds (NDEBUG) are undefined behaviour (typically a crash);
 * it is harmless only when the range is empty.
 * @warning The source and output ranges must NOT overlap; overlapping is
 * undefined behaviour.
 */
static inline tk_iterator_t
tk_algo_transform(tk_iterator_t begin, tk_iterator_t end, tk_iterator_t out,
                  void (*op)(void *out_element, const void *in_element)) {
  TK_ASSERT(begin.vtable != NULL && end.vtable != NULL &&
            "tk_algo_transform: Source iterators must not have NULL vtables.");
  TK_ASSERT(begin.vtable == end.vtable &&
            "tk_algo_transform: 'begin' and 'end' iterators are from "
            "different container types.");
  TK_ASSERT(out.vtable != NULL &&
            "tk_algo_transform: 'out' iterator must not have a NULL vtable.");
  TK_ASSERT(op != NULL && "tk_algo_transform: 'op' must not be NULL.");

  while (!tk_iter_equal(&begin, &end)) {
    void *dst = tk_iter_get(&out);
    const void *src = tk_iter_get(&begin);
    op(dst, src);
    tk_iter_next(&begin);
    tk_iter_next(&out);
  }
  return out; // one past the last element written
}

#endif // TOOLKIT_ALGO_SEQUENCE_H
