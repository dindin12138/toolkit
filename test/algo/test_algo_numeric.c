/**
 * @file test_algo_numeric.c
 * @brief Unit tests for the numeric algorithms (tk_algo_accumulate) over both
 * tk_vec_t and tk_list_t.
 *
 * This file includes the umbrella header <tk/algo/algo.h> on purpose: a
 * successful compile+link proves the umbrella exposes BOTH the sequence
 * algorithms (tk_algo_find_if) and the numeric algorithms
 * (tk_algo_accumulate) in a single include.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <tk/algo/algo.h> // umbrella: sequence.h + numeric.h
#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

// --- Helpers ---

static void int_add(void *acc, const void *element) {
  *(int *)acc += *(const int *)element;
}

static tk_bool is_30(const void *element) {
  return *(const int *)element == 30;
}

static tk_vec_t *make_int_vec(int n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 1; i <= n; ++i) {
    int val = i; // 1..n
    tk_vec_push_back(v, &val);
  }
  return v;
}

static tk_list_t *make_int_list(int n) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  for (int i = 1; i <= n; ++i) {
    int val = i; // 1..n
    tk_list_push_back(l, &val);
  }
  return l;
}

// --- accumulate over vec ---

Test(algo_numeric, accumulate_vec_sum_from_zero) {
  tk_vec_t *v = make_int_vec(5); // {1,2,3,4,5}
  int acc = 0;

  tk_algo_accumulate(tk_vec_begin(v), tk_vec_end(v), &acc, int_add);

  cr_assert_eq(acc, 15, "sum of {1..5} must be 15");
  tk_vec_destroy(v);
}

Test(algo_numeric, accumulate_vec_uses_caller_initial_value) {
  tk_vec_t *v = make_int_vec(5); // {1,2,3,4,5}
  int acc = 1000;                // initial value comes from the caller

  tk_algo_accumulate(tk_vec_begin(v), tk_vec_end(v), &acc, int_add);

  cr_assert_eq(acc, 1015, "accumulate must start from the caller's initial "
                         "value (1000 + 15)");
  tk_vec_destroy(v);
}

Test(algo_numeric, accumulate_vec_empty_range_leaves_acc_unchanged) {
  tk_vec_t *v = make_int_vec(5);
  tk_vec_clear(v); // empty range

  int acc = 42;
  tk_algo_accumulate(tk_vec_begin(v), tk_vec_end(v), &acc, int_add);

  cr_assert_eq(acc, 42, "accumulate on an empty range must not change *acc");
  tk_vec_destroy(v);
}

// --- accumulate over list ---

Test(algo_numeric, accumulate_list_sum_from_zero) {
  tk_list_t *l = make_int_list(5); // {1,2,3,4,5}
  int acc = 0;

  tk_algo_accumulate(tk_list_begin(l), tk_list_end(l), &acc, int_add);

  cr_assert_eq(acc, 15, "sum of {1..5} over a list must be 15");
  tk_list_destroy(l);
}

Test(algo_numeric, accumulate_list_uses_caller_initial_value) {
  tk_list_t *l = make_int_list(5);
  int acc = -10;

  tk_algo_accumulate(tk_list_begin(l), tk_list_end(l), &acc, int_add);

  cr_assert_eq(acc, 5, "accumulate over a list must honour the initial value "
                       "(-10 + 15)");
  tk_list_destroy(l);
}

Test(algo_numeric, accumulate_list_empty_range_leaves_acc_unchanged) {
  tk_list_t *l = make_int_list(5);
  tk_list_clear(l);

  int acc = 7;
  tk_algo_accumulate(tk_list_begin(l), tk_list_end(l), &acc, int_add);

  cr_assert_eq(acc, 7, "accumulate on an empty list must not change *acc");
  tk_list_destroy(l);
}

// --- umbrella header exposes both modules ---

Test(algo_numeric, umbrella_exposes_sequence_and_numeric) {
  tk_vec_t *v = make_int_vec(5); // {1,2,3,4,5}

  // From <tk/algo/sequence.h> via the umbrella:
  tk_iterator_t found = tk_algo_find_if(tk_vec_begin(v), tk_vec_end(v), is_30);
  // (30 is not in {1..5}, so find_if returns end.)
  tk_iterator_t end = tk_vec_end(v);
  cr_assert(tk_iter_equal(&found, &end) == true,
            "find_if from the umbrella header must behave correctly");

  // From <tk/algo/numeric.h> via the umbrella:
  int acc = 0;
  tk_algo_accumulate(tk_vec_begin(v), tk_vec_end(v), &acc, int_add);
  cr_assert_eq(acc, 15,
               "accumulate from the umbrella header must behave correctly");

  tk_vec_destroy(v);
}
