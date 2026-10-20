/**
 * @file test_algo_heap_release.c
 * @brief The release half of the heap family's contract (issue #3).
 *
 * The heap entry points take a caller violation -- a range without random
 * access, or a NULL comparator -- as follows: debug builds assert, release
 * builds RETURN IMMEDIATELY AND LEAVE THE RANGE UNTOUCHED, silently. The debug
 * half is covered by test_algo_heap.c, which cannot observe the release half
 * because it is built with assertions enabled.
 *
 * This file is compiled with NDEBUG precisely so the silent early return is
 * pinned by a test rather than living only in a comment: a refactor that turned
 * the guard into an unconditional write through a bad pointer would fail here.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include <stddef.h>

#include <tk/algo/heap.h>
#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

#ifndef NDEBUG
#error "test_algo_heap_release.c must be compiled with NDEBUG: it pins the \
release-build contract, which assertions would otherwise intercept"
#endif

// --- Helpers ---

static int int_cmp(const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return (x > y) - (x < y);
}

static tk_list_t *make_int_list(const int *values, size_t n) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  for (size_t i = 0; i < n; ++i)
    tk_list_push_back(l, &values[i]);
  return l;
}

static tk_vec_t *make_int_vec(const int *values, size_t n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = 0; i < n; ++i)
    tk_vec_push_back(v, &values[i]);
  return v;
}

static void list_to_ints(tk_list_t *l, int *out, size_t n) {
  tk_iterator_t it = tk_list_begin(l);
  tk_iterator_t end = tk_list_end(l);
  size_t i = 0;
  while (!tk_iter_equal(&it, &end) && i < n) {
    out[i++] = *(int *)tk_iter_get(&it);
    tk_iter_next(&it);
  }
}

/** Asserts the list still reads exactly `expected` (same order, same size). */
static void assert_list_unchanged(tk_list_t *l, const int *expected,
                                  size_t n) {
  cr_assert_eq(tk_list_size(l), n, "the list size must not change");
  int got[16];
  cr_assert(n <= 16, "fixture too large for the stack buffer");
  list_to_ints(l, got, n);
  for (size_t i = 0; i < n; ++i)
    cr_assert_eq(got[i], expected[i], "list element %zu was modified", i);
}

// ===========================================================================
// A bidirectional range: every entry point must return without touching it.
// ===========================================================================

Test(algo_heap_release, bidirectional_range_is_left_untouched) {
  int values[] = {3, 1, 2}; // not a heap: any write would be visible
  tk_list_t *l = make_int_list(values, 3);

  tk_algo_make_heap(tk_list_begin(l), tk_list_end(l), int_cmp);
  assert_list_unchanged(l, values, 3);

  tk_algo_push_heap(tk_list_begin(l), tk_list_end(l), int_cmp);
  assert_list_unchanged(l, values, 3);

  tk_algo_pop_heap(tk_list_begin(l), tk_list_end(l), int_cmp);
  assert_list_unchanged(l, values, 3);

  tk_algo_sort_heap(tk_list_begin(l), tk_list_end(l), int_cmp);
  assert_list_unchanged(l, values, 3);

  cr_assert(tk_algo_is_heap(tk_list_begin(l), tk_list_end(l), int_cmp) ==
                false,
            "is_heap must report false rather than asserting in release");

  tk_list_destroy(l);
}

// ===========================================================================
// A NULL comparator: same contract, on a random-access range.
// ===========================================================================

Test(algo_heap_release, null_comparator_leaves_the_range_untouched) {
  int values[] = {3, 1, 2};
  tk_vec_t *v = make_int_vec(values, 3);

  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), NULL);
  tk_algo_push_heap(tk_vec_begin(v), tk_vec_end(v), NULL);
  tk_algo_pop_heap(tk_vec_begin(v), tk_vec_end(v), NULL);
  tk_algo_sort_heap(tk_vec_begin(v), tk_vec_end(v), NULL);

  for (size_t i = 0; i < 3; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(v, i), values[i],
                 "vec element %zu was modified despite a NULL comparator", i);
  }

  cr_assert(tk_algo_is_heap(tk_vec_begin(v), tk_vec_end(v), NULL) == false,
            "is_heap must report false for a NULL comparator");

  tk_vec_destroy(v);
}
