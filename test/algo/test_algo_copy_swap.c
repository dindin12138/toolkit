/**
 * @file test_algo_copy_swap.c
 * @brief Tests for the element-moving algorithms that rely on the iterator's
 * element size (issue #2): tk_algo_copy (which no longer takes a size) and the
 * new public tk_algo_swap.
 *
 * The element types are chosen to catch the two ways an inferred size can go
 * wrong:
 *  - a 40-byte struct (bigger than a pointer) catches "size == sizeof(void*)";
 *  - a 13-byte struct catches "swap assumes pointer alignment", because in a
 *    vec of 13-byte elements the second element starts at an odd address.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include <signal.h>   // SIGABRT
#include <stdint.h>   // int64_t
#include <sys/wait.h> // waitpid, WIFSIGNALED, WTERMSIG
#include <unistd.h>   // fork, _exit, pid_t

#include <tk/algo/sequence.h>
#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

// --- Element types ---

typedef struct {
  int64_t v[5];
} big40_t; // 40 bytes: larger than a pointer

typedef struct {
  unsigned char b[13];
} odd13_t; // 13 bytes: consecutive vec elements are misaligned

// --- Fixtures ---

static tk_vec_t *make_int_vec(const int *values, size_t n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = 0; i < n; ++i)
    tk_vec_push_back(v, &values[i]);
  return v;
}

static tk_vec_t *make_zero_int_vec(size_t n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = 0; i < n; ++i) {
    int zero = 0;
    tk_vec_push_back(v, &zero);
  }
  return v;
}

static tk_list_t *make_int_list(const int *values, size_t n) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  for (size_t i = 0; i < n; ++i)
    tk_list_push_back(l, &values[i]);
  return l;
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

// --- copy: the size comes from the source iterator ---

Test(algo_copy_swap, copy_infers_element_size_from_source) {
  int src_values[] = {10, 20, 30};
  tk_vec_t *src = make_int_vec(src_values, 3);
  tk_vec_t *dst = make_zero_int_vec(3);

  // Three arguments: no element size at the call site.
  tk_iterator_t ret =
      tk_algo_copy(tk_vec_begin(src), tk_vec_end(src), tk_vec_begin(dst));

  for (size_t i = 0; i < 3; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(dst, i), src_values[i],
                 "copy mismatch at index %zu", i);
  }
  tk_iterator_t dst_end = tk_vec_end(dst);
  cr_assert(tk_iter_equal(&ret, &dst_end) == true,
            "copy must return the output iterator past the last write");

  tk_vec_destroy(src);
  tk_vec_destroy(dst);
}

Test(algo_copy_swap, copy_list_source_into_vec_without_size) {
  int src_values[] = {10, 20, 30};
  tk_list_t *src = make_int_list(src_values, 3);
  tk_vec_t *dst = make_zero_int_vec(3);

  (void)tk_algo_copy(tk_list_begin(src), tk_list_end(src), tk_vec_begin(dst));

  for (size_t i = 0; i < 3; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(dst, i), src_values[i],
                 "list->vec copy mismatch at index %zu", i);
  }

  tk_list_destroy(src);
  tk_vec_destroy(dst);
}

Test(algo_copy_swap, copy_large_struct_element) {
  // 40-byte elements: a size inferred as sizeof(void*) would copy only 8 bytes
  // per element and leave the rest of every destination element untouched.
  tk_vec_t *src = tk_vec_create(sizeof(big40_t));
  cr_assert_not_null(src);
  tk_vec_t *dst = tk_vec_create(sizeof(big40_t));
  cr_assert_not_null(dst);

  for (int i = 0; i < 3; ++i) {
    big40_t s;
    for (int k = 0; k < 5; ++k)
      s.v[k] = (int64_t)(100 * i + k);
    tk_vec_push_back(src, &s);
    big40_t zero;
    for (int k = 0; k < 5; ++k)
      zero.v[k] = -1;
    tk_vec_push_back(dst, &zero);
  }

  (void)tk_algo_copy(tk_vec_begin(src), tk_vec_end(src), tk_vec_begin(dst));

  for (int i = 0; i < 3; ++i) {
    const big40_t *got = (const big40_t *)tk_vec_at(dst, (size_t)i);
    for (int k = 0; k < 5; ++k) {
      cr_assert_eq(got->v[k], (int64_t)(100 * i + k),
                   "40-byte element %d field %d was not copied whole", i, k);
    }
  }

  tk_vec_destroy(src);
  tk_vec_destroy(dst);
}

// --- swap: exchanges two elements byte for byte ---

Test(algo_copy_swap, swap_ints_in_vec) {
  int values[] = {1, 2, 3, 4, 5};
  tk_vec_t *v = make_int_vec(values, 5);

  tk_iterator_t first = tk_vec_begin(v);
  tk_iterator_t last = tk_vec_end(v);
  tk_iter_prev(&last);

  tk_algo_swap(first, last);

  int expected[] = {5, 2, 3, 4, 1};
  for (size_t i = 0; i < 5; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(v, i), expected[i],
                 "swap mismatch at index %zu", i);
  }

  tk_vec_destroy(v);
}

Test(algo_copy_swap, swap_list_elements) {
  int values[] = {1, 2, 3};
  tk_list_t *l = make_int_list(values, 3);

  tk_iterator_t first = tk_list_begin(l);
  tk_iterator_t last = tk_list_end(l);
  tk_iter_prev(&last); // bidirectional: end() can retreat to the tail

  tk_algo_swap(first, last);

  int got[3] = {0};
  list_to_ints(l, got, 3);
  cr_assert_eq(got[0], 3, "swap must move the last element to the front");
  cr_assert_eq(got[1], 2, "swap must leave the middle element alone");
  cr_assert_eq(got[2], 1, "swap must move the first element to the back");

  tk_list_destroy(l);
}

Test(algo_copy_swap, swap_large_struct_element) {
  tk_vec_t *v = tk_vec_create(sizeof(big40_t));
  cr_assert_not_null(v);
  big40_t a, b;
  for (int k = 0; k < 5; ++k) {
    a.v[k] = (int64_t)k;      // 0..4
    b.v[k] = (int64_t)(100 + k); // 100..104
  }
  tk_vec_push_back(v, &a);
  tk_vec_push_back(v, &b);

  tk_iterator_t i0 = tk_vec_begin(v);
  tk_iterator_t i1 = tk_vec_begin(v);
  tk_iter_advance_by(&i1, 1);
  tk_algo_swap(i0, i1);

  const big40_t *e0 = (const big40_t *)tk_vec_at(v, 0);
  const big40_t *e1 = (const big40_t *)tk_vec_at(v, 1);
  for (int k = 0; k < 5; ++k) {
    cr_assert_eq(e0->v[k], (int64_t)(100 + k), "element 0 field %d", k);
    cr_assert_eq(e1->v[k], (int64_t)k, "element 1 field %d", k);
  }

  tk_vec_destroy(v);
}

Test(algo_copy_swap, swap_odd_sized_unaligned_elements) {
  // 13-byte elements: element 1 starts at offset 13, which is not pointer
  // aligned. A swap that casts to a wider type would be undefined behaviour
  // here; a byte-wise swap is exact.
  cr_assert_eq(sizeof(odd13_t), (size_t)13, "odd13_t must be 13 bytes");

  tk_vec_t *v = tk_vec_create(sizeof(odd13_t));
  cr_assert_not_null(v);
  odd13_t a, b, c;
  for (int i = 0; i < 13; ++i) {
    a.b[i] = (unsigned char)(0xA0 + i);
    b.b[i] = (unsigned char)(0xB0 + i);
    c.b[i] = (unsigned char)(0xC0 + i);
  }
  tk_vec_push_back(v, &a);
  tk_vec_push_back(v, &b);
  tk_vec_push_back(v, &c);

  tk_iterator_t i0 = tk_vec_begin(v);
  tk_iterator_t i2 = tk_vec_begin(v);
  tk_iter_advance_by(&i2, 2);
  tk_algo_swap(i0, i2);

  const odd13_t *e0 = (const odd13_t *)tk_vec_at(v, 0);
  const odd13_t *e1 = (const odd13_t *)tk_vec_at(v, 1);
  const odd13_t *e2 = (const odd13_t *)tk_vec_at(v, 2);
  for (int i = 0; i < 13; ++i) {
    cr_assert_eq(e0->b[i], (unsigned char)(0xC0 + i),
                 "element 0 byte %d must come from element 2", i);
    cr_assert_eq(e1->b[i], (unsigned char)(0xB0 + i),
                 "element 1 byte %d must be untouched (no overrun into a "
                 "neighbour)",
                 i);
    cr_assert_eq(e2->b[i], (unsigned char)(0xA0 + i),
                 "element 2 byte %d must come from element 0", i);
  }

  tk_vec_destroy(v);
}

Test(algo_copy_swap, swap_same_position_is_noop) {
  int values[] = {7, 8};
  tk_vec_t *v = make_int_vec(values, 2);

  tk_iterator_t it = tk_vec_begin(v);
  tk_algo_swap(it, it);

  cr_assert_eq(*(int *)tk_vec_at(v, 0), 7);
  cr_assert_eq(*(int *)tk_vec_at(v, 1), 8);

  tk_vec_destroy(v);
}

// ===========================================================================
// Debug-only: mismatched element sizes must trip an assertion rather than
// silently truncating or overrunning. TK_ASSERT aborts, so the call runs in a
// forked grandchild (same pattern as the vtable-validator tests).
// ===========================================================================
#ifndef NDEBUG

// These run in a forked grandchild, where Criterion's assertion machinery is
// not available: failures are reported by _exit(2) instead of cr_assert.
static void copy_across_mismatched_sizes(void) {
  tk_vec_t *src = tk_vec_create(sizeof(int)); // 4-byte elements
  tk_vec_t *dst = tk_vec_create(sizeof(int64_t)); // 8-byte elements
  if (!src || !dst)
    _exit(2);

  int a = 1, b = 2;
  int64_t zero = 0;
  tk_vec_push_back(src, &a);
  tk_vec_push_back(src, &b);
  tk_vec_push_back(dst, &zero);
  tk_vec_push_back(dst, &zero);

  (void)tk_algo_copy(tk_vec_begin(src), tk_vec_end(src), tk_vec_begin(dst));
}

static void swap_across_mismatched_sizes(void) {
  tk_vec_t *va = tk_vec_create(sizeof(int)); // 4-byte elements
  tk_vec_t *vb = tk_vec_create(sizeof(int64_t)); // 8-byte elements
  if (!va || !vb)
    _exit(2);

  int a = 1;
  int64_t b = 2;
  tk_vec_push_back(va, &a);
  tk_vec_push_back(vb, &b);

  tk_algo_swap(tk_vec_begin(va), tk_vec_begin(vb));
}

// Runs `fn` in a forked grandchild; true iff it was killed by SIGABRT.
static tk_bool aborts(void (*fn)(void)) {
  pid_t pid = fork();
  cr_assert(pid >= 0, "fork() failed");
  if (pid == 0) {
    fn(); // must assert
    _exit(0);
  }
  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  cr_assert_eq(waited, pid, "waitpid() failed");
  return WIFSIGNALED(status) && (WTERMSIG(status) == SIGABRT);
}

Test(algo_copy_swap, copy_mismatched_element_sizes_asserts) {
  cr_assert(aborts(copy_across_mismatched_sizes),
            "copying between 4-byte and 8-byte elements must assert");
}

Test(algo_copy_swap, swap_mismatched_element_sizes_asserts) {
  cr_assert(aborts(swap_across_mismatched_sizes),
            "swapping 4-byte and 8-byte elements must assert");
}

#endif // !NDEBUG
