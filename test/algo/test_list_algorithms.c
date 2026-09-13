/**
 * @file test_list_algorithms.c
 * @brief Unit tests for the sequence algorithms
 * (tk_algo_for_each / count / count_if / copy / transform) driven by a
 * tk_list_t (a bidirectional container).
 *
 * This mirrors test_vec_algorithms.c to demonstrate that the SAME generic
 * algorithms work over a container with a completely different internal
 * structure, purely through the polymorphic tk_iterator_t interface.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <tk/algo/sequence.h>
#include <tk/core/iterator.h>
#include <tk/ds/list.h>

// --- Test fixture: {10, 20, 30, 40, 50} ---

static tk_list_t *list_int;

void setup_list_algo_suite2(void) {
  list_int = tk_list_create(sizeof(int));
  cr_assert_not_null(list_int, "List creation failed");
  for (int i = 1; i <= 5; ++i) {
    int val = i * 10;
    tk_list_push_back(list_int, &val);
  }
}

void teardown_list_algo_suite2(void) { tk_list_destroy(list_int); }

TestSuite(list_algo_suite2, .init = setup_list_algo_suite2,
          .fini = teardown_list_algo_suite2);

// --- Helpers ---

static int g_list_for_each_calls = 0;

static void list_add_one(void *element) {
  *(int *)element += 1;
  ++g_list_for_each_calls;
}

static void list_double_it(void *out_element, const void *in_element) {
  *(int *)out_element = *(const int *)in_element * 2;
}

static tk_bool list_is_odd(const void *element) {
  return (*(const int *)element) % 2 != 0;
}

static tk_bool list_greater_than_25(const void *element) {
  return (*(const int *)element) > 25;
}

static tk_bool list_int_eq(const void *element, const void *value) {
  return *(const int *)element == *(const int *)value;
}

// Build a pre-sized list of `n` zero elements so its iterators are
// dereferenceable (a list iterator needs an existing node).
static tk_list_t *make_zero_list(int n) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  for (int i = 0; i < n; ++i) {
    int zero = 0;
    tk_list_push_back(l, &zero);
  }
  return l;
}

// Collect up to `n` elements of a list into `out` via the iterator interface.
static size_t list_to_array(tk_list_t *l, int *out, size_t n) {
  tk_iterator_t it = tk_list_begin(l);
  tk_iterator_t end = tk_list_end(l);
  size_t i = 0;
  while (!tk_iter_equal(&it, &end) && i < n) {
    out[i++] = *(int *)tk_iter_get(&it);
    tk_iter_next(&it);
  }
  return i;
}

// --- for_each ---

Test(list_algo_suite2, for_each_mutates_in_place) {
  g_list_for_each_calls = 0;
  tk_iterator_t begin = tk_list_begin(list_int);
  tk_iterator_t end = tk_list_end(list_int);

  tk_algo_for_each(begin, end, list_add_one);

  cr_assert_eq(g_list_for_each_calls, 5,
               "for_each must call f once per element");
  int got[5] = {0};
  size_t n = list_to_array(list_int, got, 5);
  cr_assert_eq(n, (size_t)5);
  int expected[] = {11, 21, 31, 41, 51};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(got[i], expected[i], "for_each mutation mismatch at %d", i);
  }
}

Test(list_algo_suite2, for_each_empty_range_does_not_call_f) {
  tk_list_clear(list_int);
  g_list_for_each_calls = 0;

  tk_iterator_t begin = tk_list_begin(list_int);
  tk_iterator_t end = tk_list_end(list_int);
  tk_algo_for_each(begin, end, list_add_one);

  cr_assert_eq(g_list_for_each_calls, 0,
               "for_each on an empty range must not call f");
}

// --- count_if ---

Test(list_algo_suite2, count_if_hits_and_misses) {
  tk_iterator_t begin = tk_list_begin(list_int);
  tk_iterator_t end = tk_list_end(list_int);

  cr_assert_eq(tk_algo_count_if(begin, end, list_greater_than_25), (size_t)3,
               "count_if(>25) must be 3");
  cr_assert_eq(tk_algo_count_if(begin, end, list_is_odd), (size_t)0,
               "count_if(odd) must be 0");
}

Test(list_algo_suite2, count_if_empty_range_is_zero) {
  tk_list_clear(list_int);
  tk_iterator_t begin = tk_list_begin(list_int);
  tk_iterator_t end = tk_list_end(list_int);

  cr_assert_eq(tk_algo_count_if(begin, end, list_greater_than_25), (size_t)0,
               "count_if on an empty range must be 0");
}

// --- count ---

Test(list_algo_suite2, count_hits_and_misses) {
  tk_iterator_t begin = tk_list_begin(list_int);
  tk_iterator_t end = tk_list_end(list_int);

  int target = 30;
  cr_assert_eq(tk_algo_count(begin, end, &target, list_int_eq), (size_t)1,
               "count(==30) must be 1");
  int missing = 99;
  cr_assert_eq(tk_algo_count(begin, end, &missing, list_int_eq), (size_t)0,
               "count(==99) must be 0");
}

Test(list_algo_suite2, count_empty_range_is_zero) {
  tk_list_clear(list_int);
  tk_iterator_t begin = tk_list_begin(list_int);
  tk_iterator_t end = tk_list_end(list_int);

  int target = 30;
  cr_assert_eq(tk_algo_count(begin, end, &target, list_int_eq), (size_t)0,
               "count on an empty range must be 0");
}

// --- copy ---

Test(list_algo_suite2, copy_list_to_list_content_and_return) {
  tk_list_t *dst = make_zero_list(5);

  tk_iterator_t src_begin = tk_list_begin(list_int);
  tk_iterator_t src_end = tk_list_end(list_int);
  tk_iterator_t out = tk_list_begin(dst);

  tk_iterator_t ret = tk_algo_copy(src_begin, src_end, out, sizeof(int));

  int got[5] = {0};
  size_t n = list_to_array(dst, got, 5);
  cr_assert_eq(n, (size_t)5);
  int expected[] = {10, 20, 30, 40, 50};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(got[i], expected[i], "copy content mismatch at %d", i);
  }

  tk_iterator_t dst_end = tk_list_end(dst);
  cr_assert(tk_iter_equal(&ret, &dst_end) == true,
            "copy must return the output iterator past the last written "
            "element");

  tk_list_destroy(dst);
}

Test(list_algo_suite2, copy_empty_range_returns_out) {
  tk_list_clear(list_int);
  tk_list_t *dst = make_zero_list(3);

  tk_iterator_t src_begin = tk_list_begin(list_int);
  tk_iterator_t src_end = tk_list_end(list_int);
  tk_iterator_t out = tk_list_begin(dst);
  tk_iterator_t out_orig = out;

  tk_iterator_t ret = tk_algo_copy(src_begin, src_end, out, sizeof(int));

  cr_assert(tk_iter_equal(&ret, &out_orig) == true,
            "copy of an empty range must return the incoming out iterator");

  int got[3] = {-1, -1, -1};
  (void)list_to_array(dst, got, 3);
  for (int i = 0; i < 3; ++i) {
    cr_assert_eq(got[i], 0, "empty copy must not modify the destination");
  }

  tk_list_destroy(dst);
}

// --- transform ---

Test(list_algo_suite2, transform_list_to_list_content_and_return) {
  tk_list_t *dst = make_zero_list(5);

  tk_iterator_t src_begin = tk_list_begin(list_int);
  tk_iterator_t src_end = tk_list_end(list_int);
  tk_iterator_t out = tk_list_begin(dst);

  tk_iterator_t ret =
      tk_algo_transform(src_begin, src_end, out, list_double_it);

  int got[5] = {0};
  size_t n = list_to_array(dst, got, 5);
  cr_assert_eq(n, (size_t)5);
  int expected[] = {20, 40, 60, 80, 100};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(got[i], expected[i], "transform content mismatch at %d", i);
  }

  tk_iterator_t dst_end = tk_list_end(dst);
  cr_assert(tk_iter_equal(&ret, &dst_end) == true,
            "transform must return the output iterator past the last written "
            "element");

  tk_list_destroy(dst);
}

Test(list_algo_suite2, transform_empty_range_returns_out) {
  tk_list_clear(list_int);
  tk_list_t *dst = make_zero_list(3);

  tk_iterator_t src_begin = tk_list_begin(list_int);
  tk_iterator_t src_end = tk_list_end(list_int);
  tk_iterator_t out = tk_list_begin(dst);
  tk_iterator_t out_orig = out;

  tk_iterator_t ret =
      tk_algo_transform(src_begin, src_end, out, list_double_it);

  cr_assert(tk_iter_equal(&ret, &out_orig) == true,
            "transform of an empty range must return the incoming out "
            "iterator");

  tk_list_destroy(dst);
}
