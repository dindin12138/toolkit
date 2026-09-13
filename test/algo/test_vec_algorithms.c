/**
 * @file test_vec_algorithms.c
 * @brief Unit tests for the modifying/non-modifying sequence algorithms
 * (tk_algo_for_each / count / count_if / copy / transform) driven by a
 * tk_vec_t (a random-access container).
 *
 * The vector is used purely through the polymorphic tk_iterator_t interface,
 * proving the algorithms do not depend on the concrete container.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <tk/algo/sequence.h>
#include <tk/core/iterator.h>
#include <tk/ds/vec.h>

// --- Test fixture: {10, 20, 30, 40, 50} ---

static tk_vec_t *vec_int;

void setup_vec_algo_suite(void) {
  vec_int = tk_vec_create(sizeof(int));
  cr_assert_not_null(vec_int, "Vector creation failed");
  for (int i = 1; i <= 5; ++i) {
    int val = i * 10;
    tk_vec_push_back(vec_int, &val);
  }
}

void teardown_vec_algo_suite(void) { tk_vec_destroy(vec_int); }

TestSuite(vec_algo_suite, .init = setup_vec_algo_suite,
          .fini = teardown_vec_algo_suite);

// --- Helpers ---

static int g_for_each_calls = 0;

static void add_one(void *element) {
  *(int *)element += 1;
  ++g_for_each_calls;
}

static void double_it(void *out_element, const void *in_element) {
  *(int *)out_element = *(const int *)in_element * 2;
}

static tk_bool is_odd(const void *element) {
  return (*(const int *)element) % 2 != 0;
}

static tk_bool greater_than_25(const void *element) {
  return (*(const int *)element) > 25;
}

static tk_bool int_eq(const void *element, const void *value) {
  return *(const int *)element == *(const int *)value;
}

// Build a pre-sized destination vector of `n` zero elements so that its
// iterators are dereferenceable (a vec iterator needs existing storage).
static tk_vec_t *make_zero_vec(int n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 0; i < n; ++i) {
    int zero = 0;
    tk_vec_push_back(v, &zero);
  }
  return v;
}

// --- for_each ---

Test(vec_algo_suite, for_each_mutates_in_place) {
  g_for_each_calls = 0;
  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  tk_algo_for_each(begin, end, add_one);

  cr_assert_eq(g_for_each_calls, 5, "for_each must call f once per element");
  int expected[] = {11, 21, 31, 41, 51};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(vec_int, (size_t)i), expected[i],
                 "for_each mutation mismatch at index %d", i);
  }
}

Test(vec_algo_suite, for_each_empty_range_does_not_call_f) {
  tk_vec_clear(vec_int);
  g_for_each_calls = 0;

  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);
  tk_algo_for_each(begin, end, add_one);

  cr_assert_eq(g_for_each_calls, 0,
               "for_each on an empty range must not call f");
}

// --- count_if ---

Test(vec_algo_suite, count_if_hits_and_misses) {
  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  cr_assert_eq(tk_algo_count_if(begin, end, greater_than_25), (size_t)3,
               "count_if(>25) over {10,20,30,40,50} must be 3");
  cr_assert_eq(tk_algo_count_if(begin, end, is_odd), (size_t)0,
               "count_if(odd) over {10,20,30,40,50} must be 0");
}

Test(vec_algo_suite, count_if_empty_range_is_zero) {
  tk_vec_clear(vec_int);
  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  cr_assert_eq(tk_algo_count_if(begin, end, greater_than_25), (size_t)0,
               "count_if on an empty range must be 0");
}

// --- count ---

Test(vec_algo_suite, count_hits_and_misses) {
  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  int target = 30;
  cr_assert_eq(tk_algo_count(begin, end, &target, int_eq), (size_t)1,
               "count(==30) must be 1");
  int missing = 99;
  cr_assert_eq(tk_algo_count(begin, end, &missing, int_eq), (size_t)0,
               "count(==99) must be 0");
}

Test(vec_algo_suite, count_empty_range_is_zero) {
  tk_vec_clear(vec_int);
  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  int target = 30;
  cr_assert_eq(tk_algo_count(begin, end, &target, int_eq), (size_t)0,
               "count on an empty range must be 0");
}

// --- copy ---

Test(vec_algo_suite, copy_vec_to_vec_content_and_return) {
  tk_vec_t *dst = make_zero_vec(5);

  tk_iterator_t src_begin = tk_vec_begin(vec_int);
  tk_iterator_t src_end = tk_vec_end(vec_int);
  tk_iterator_t out = tk_vec_begin(dst);

  tk_iterator_t ret = tk_algo_copy(src_begin, src_end, out, sizeof(int));

  int expected[] = {10, 20, 30, 40, 50};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(dst, (size_t)i), expected[i],
                 "copy content mismatch at index %d", i);
  }

  // The return value must be one past the last written element, i.e. end(dst)
  // when the destination is exactly the same length as the source.
  tk_iterator_t dst_end = tk_vec_end(dst);
  cr_assert(tk_iter_equal(&ret, &dst_end) == true,
            "copy must return the output iterator past the last written "
            "element");

  tk_vec_destroy(dst);
}

Test(vec_algo_suite, copy_empty_range_returns_out) {
  tk_vec_clear(vec_int);
  tk_vec_t *dst = make_zero_vec(3);

  tk_iterator_t src_begin = tk_vec_begin(vec_int);
  tk_iterator_t src_end = tk_vec_end(vec_int);
  tk_iterator_t out = tk_vec_begin(dst);
  tk_iterator_t out_orig = out;

  tk_iterator_t ret = tk_algo_copy(src_begin, src_end, out, sizeof(int));

  cr_assert(tk_iter_equal(&ret, &out_orig) == true,
            "copy of an empty range must return the incoming out iterator");

  // Destination untouched.
  for (int i = 0; i < 3; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(dst, (size_t)i), 0,
                 "empty copy must not modify the destination");
  }

  tk_vec_destroy(dst);
}

// --- transform ---

Test(vec_algo_suite, transform_vec_to_vec_content_and_return) {
  tk_vec_t *dst = make_zero_vec(5);

  tk_iterator_t src_begin = tk_vec_begin(vec_int);
  tk_iterator_t src_end = tk_vec_end(vec_int);
  tk_iterator_t out = tk_vec_begin(dst);

  tk_iterator_t ret = tk_algo_transform(src_begin, src_end, out, double_it);

  int expected[] = {20, 40, 60, 80, 100};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(dst, (size_t)i), expected[i],
                 "transform content mismatch at index %d", i);
  }

  tk_iterator_t dst_end = tk_vec_end(dst);
  cr_assert(tk_iter_equal(&ret, &dst_end) == true,
            "transform must return the output iterator past the last written "
            "element");

  tk_vec_destroy(dst);
}

Test(vec_algo_suite, transform_empty_range_returns_out) {
  tk_vec_clear(vec_int);
  tk_vec_t *dst = make_zero_vec(3);

  tk_iterator_t src_begin = tk_vec_begin(vec_int);
  tk_iterator_t src_end = tk_vec_end(vec_int);
  tk_iterator_t out = tk_vec_begin(dst);
  tk_iterator_t out_orig = out;

  tk_iterator_t ret = tk_algo_transform(src_begin, src_end, out, double_it);

  cr_assert(tk_iter_equal(&ret, &out_orig) == true,
            "transform of an empty range must return the incoming out "
            "iterator");

  tk_vec_destroy(dst);
}
