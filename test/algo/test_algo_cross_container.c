/**
 * @file test_algo_cross_container.c
 * @brief Cross-container integration tests: the generic algorithms are driven
 * with a tk_list_t source and a tk_vec_t destination (and vice versa).
 *
 * This is the central proof that the algorithms are genuinely decoupled from
 * the container: the SAME tk_algo_copy / tk_algo_transform call reads from a
 * node-based linked list and writes into a contiguous dynamic array, because
 * both only ever talk to the tk_iterator_t interface.
 *
 * It also runs a small "polymorphism matrix": the same set of assertions is
 * executed once over a vector and once over a list, asserting identical
 * results.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <tk/algo/algo.h>
#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

// --- Helpers ---

static void double_it(void *out_element, const void *in_element) {
  *(int *)out_element = *(const int *)in_element * 2;
}

static void add_one(void *element) { *(int *)element += 1; }

static void int_add(void *acc, const void *element) {
  *(int *)acc += *(const int *)element;
}

static tk_bool greater_than_25(const void *element) {
  return (*(const int *)element) > 25;
}

static tk_bool int_eq(const void *element, const void *value) {
  return *(const int *)element == *(const int *)value;
}

static tk_bool is_30(const void *element) {
  return *(const int *)element == 30;
}

// {10,20,30,40,50}
static tk_vec_t *make_src_vec(void) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 1; i <= 5; ++i) {
    int val = i * 10;
    tk_vec_push_back(v, &val);
  }
  return v;
}

static tk_list_t *make_src_list(void) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  for (int i = 1; i <= 5; ++i) {
    int val = i * 10;
    tk_list_push_back(l, &val);
  }
  return l;
}

static tk_vec_t *make_zero_vec(int n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 0; i < n; ++i) {
    int zero = 0;
    tk_vec_push_back(v, &zero);
  }
  return v;
}

static tk_list_t *make_zero_list(int n) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  for (int i = 0; i < n; ++i) {
    int zero = 0;
    tk_list_push_back(l, &zero);
  }
  return l;
}

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

// --- Cross-container copy: list -> vec ---

Test(algo_cross_container, copy_list_source_into_vec_destination) {
  tk_list_t *src = make_src_list(); // {10,20,30,40,50}
  tk_vec_t *dst = make_zero_vec(5);

  // Source iterators come from the LIST; the output iterator from the VEC.
  tk_iterator_t ret = tk_algo_copy(tk_list_begin(src), tk_list_end(src),
                                   tk_vec_begin(dst), sizeof(int));

  int expected[] = {10, 20, 30, 40, 50};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(dst, (size_t)i), expected[i],
                 "list->vec copy mismatch at index %d", i);
  }

  tk_iterator_t dst_end = tk_vec_end(dst);
  cr_assert(tk_iter_equal(&ret, &dst_end) == true,
            "list->vec copy must return the vec output iterator past the last "
            "write");

  tk_list_destroy(src);
  tk_vec_destroy(dst);
}

// --- Cross-container transform: list -> vec ---

Test(algo_cross_container, transform_list_source_into_vec_destination) {
  tk_list_t *src = make_src_list(); // {10,20,30,40,50}
  tk_vec_t *dst = make_zero_vec(5);

  tk_iterator_t ret = tk_algo_transform(tk_list_begin(src), tk_list_end(src),
                                        tk_vec_begin(dst), double_it);

  int expected[] = {20, 40, 60, 80, 100};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(dst, (size_t)i), expected[i],
                 "list->vec transform mismatch at index %d", i);
  }

  tk_iterator_t dst_end = tk_vec_end(dst);
  cr_assert(tk_iter_equal(&ret, &dst_end) == true,
            "list->vec transform must return the vec output iterator past the "
            "last write");

  tk_list_destroy(src);
  tk_vec_destroy(dst);
}

// --- Cross-container copy: vec -> list ---

Test(algo_cross_container, copy_vec_source_into_list_destination) {
  tk_vec_t *src = make_src_vec(); // {10,20,30,40,50}
  tk_list_t *dst = make_zero_list(5);

  tk_iterator_t ret = tk_algo_copy(tk_vec_begin(src), tk_vec_end(src),
                                   tk_list_begin(dst), sizeof(int));

  int got[5] = {0};
  size_t n = list_to_array(dst, got, 5);
  cr_assert_eq(n, (size_t)5);
  int expected[] = {10, 20, 30, 40, 50};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(got[i], expected[i], "vec->list copy mismatch at index %d", i);
  }

  tk_iterator_t dst_end = tk_list_end(dst);
  cr_assert(tk_iter_equal(&ret, &dst_end) == true,
            "vec->list copy must return the list output iterator past the last "
            "write");

  tk_vec_destroy(src);
  tk_list_destroy(dst);
}

// --- Polymorphism matrix: identical results over vec and list ---

Test(algo_cross_container, same_results_on_vec_and_list) {
  tk_vec_t *v = make_src_vec();  // {10,20,30,40,50}
  tk_list_t *l = make_src_list(); // {10,20,30,40,50}

  // find_if: 30 exists in both.
  tk_iterator_t vf = tk_algo_find_if(tk_vec_begin(v), tk_vec_end(v), is_30);
  tk_iterator_t lf = tk_algo_find_if(tk_list_begin(l), tk_list_end(l), is_30);
  cr_assert_eq(*(int *)tk_iter_get(&vf), 30);
  cr_assert_eq(*(int *)tk_iter_get(&lf), 30);

  // count_if(>25) == 3 on both.
  size_t vcif = tk_algo_count_if(tk_vec_begin(v), tk_vec_end(v),
                                 greater_than_25);
  size_t lcif = tk_algo_count_if(tk_list_begin(l), tk_list_end(l),
                                 greater_than_25);
  cr_assert_eq(vcif, (size_t)3);
  cr_assert_eq(lcif, (size_t)3);

  // count(==30) == 1 on both.
  int target = 30;
  size_t vc = tk_algo_count(tk_vec_begin(v), tk_vec_end(v), &target, int_eq);
  size_t lc = tk_algo_count(tk_list_begin(l), tk_list_end(l), &target, int_eq);
  cr_assert_eq(vc, (size_t)1);
  cr_assert_eq(lc, (size_t)1);

  // accumulate == 150 on both.
  int vsum = 0;
  int lsum = 0;
  tk_algo_accumulate(tk_vec_begin(v), tk_vec_end(v), &vsum, int_add);
  tk_algo_accumulate(tk_list_begin(l), tk_list_end(l), &lsum, int_add);
  cr_assert_eq(vsum, 150);
  cr_assert_eq(lsum, 150);

  // for_each(+1) mutates both to {11,21,31,41,51}.
  tk_algo_for_each(tk_vec_begin(v), tk_vec_end(v), add_one);
  tk_algo_for_each(tk_list_begin(l), tk_list_end(l), add_one);
  int vgot[5] = {0};
  int lgot[5] = {0};
  for (int i = 0; i < 5; ++i)
    vgot[i] = *(int *)tk_vec_at(v, (size_t)i);
  (void)list_to_array(l, lgot, 5);
  int expected[] = {11, 21, 31, 41, 51};
  for (int i = 0; i < 5; ++i) {
    cr_assert_eq(vgot[i], expected[i]);
    cr_assert_eq(lgot[i], expected[i]);
  }

  tk_vec_destroy(v);
  tk_list_destroy(l);
}
