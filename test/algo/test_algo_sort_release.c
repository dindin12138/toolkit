/**
 * @file test_algo_sort_release.c
 * @brief The release half of tk_algo_sort's contract (issue #4).
 *
 * The sort takes a caller violation -- a range without random access, or a NULL
 * comparator -- as follows: debug builds assert, release builds RETURN
 * IMMEDIATELY AND LEAVE THE RANGE UNTOUCHED, silently. The debug half is
 * covered by test_algo_sort.c, which cannot observe the release half because it
 * is built with assertions enabled.
 *
 * This file is compiled with NDEBUG precisely so the silent early return is
 * pinned by a test rather than living only in a comment. That matters more here
 * than for the heap family: a sort that silently does nothing is
 * indistinguishable, at the call site, from a sort of an already ordered range,
 * so the header shouts about it and this test keeps the shout honest. A
 * refactor that turned the guard into an unconditional walk over a
 * bidirectional range would fail here instead of corrupting a caller's list.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include <stddef.h>

#include <tk/algo/sort.h>
#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

#ifndef NDEBUG
#error "test_algo_sort_release.c must be compiled with NDEBUG: it pins the \
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

/** Asserts the list still reads exactly `expected` (same order, same size). */
static void assert_list_unchanged(tk_list_t *l, const int *expected,
                                  size_t n) {
  cr_assert_eq(tk_list_size(l), n, "the list size must not change");

  tk_iterator_t it = tk_list_begin(l);
  tk_iterator_t end = tk_list_end(l);
  size_t i = 0;
  while (!tk_iter_equal(&it, &end)) {
    cr_assert(i < n, "the list grew");
    int got = *(int *)tk_iter_get(&it);
    cr_assert_eq(got, expected[i], "list element %zu was modified", i);
    ++i;
    tk_iter_next(&it);
  }
  cr_assert_eq(i, n, "the list shrank");
}

// ===========================================================================
// A bidirectional range: the sort must return without touching it.
// ===========================================================================

Test(algo_sort_release, bidirectional_range_is_left_untouched) {
  int values[] = {4, 1, 3, 2}; // unsorted: any write would be visible
  tk_list_t *l = make_int_list(values, 4);

  tk_algo_sort(tk_list_begin(l), tk_list_end(l), int_cmp);

  assert_list_unchanged(l, values, 4);

  tk_list_destroy(l);
}

// ===========================================================================
// A NULL comparator: same contract, on a random-access range.
// ===========================================================================

Test(algo_sort_release, null_comparator_leaves_the_range_untouched) {
  int values[] = {4, 1, 3, 2};
  tk_vec_t *v = make_int_vec(values, 4);

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), NULL);

  cr_assert_eq(tk_vec_size(v), (size_t)4, "the range size must not change");
  for (size_t i = 0; i < 4; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(v, i), values[i],
                 "vec element %zu was modified despite a NULL comparator", i);
  }

  tk_vec_destroy(v);
}
