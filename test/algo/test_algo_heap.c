/**
 * @file test_algo_heap.c
 * @brief Tests for the heap family (issue #3): tk_algo_make_heap,
 * tk_algo_push_heap, tk_algo_pop_heap, tk_algo_sort_heap and tk_algo_is_heap.
 *
 * Every assertion is made through the public algorithm headers, driving
 * iterators obtained from a real container. Nothing here inspects the heap's
 * internal sequence of sifts: only the arrangement of the range afterwards.
 *
 * The heap property is never verified with the library's own tk_algo_is_heap
 * when the point is "the range is a heap" -- that would let the checker and the
 * implementation share one misunderstanding. `array_is_heap` below is written
 * directly on a plain int array and is the independent source of truth.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include <signal.h>   // SIGABRT
#include <stddef.h>   // size_t
#include <sys/wait.h> // waitpid, WIFSIGNALED, WTERMSIG
#include <unistd.h>   // fork, _exit, pid_t

#include <tk/algo/heap.h>
#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

// --- Comparators and independent checkers ---

/** Three-way ascending comparator for int elements (qsort convention). */
static int int_cmp(const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return (x > y) - (x < y);
}

/**
 * @brief Independent heap-property checker over a plain array.
 *
 * Deliberately does not call into the library: it walks parent/child index
 * pairs itself. A max-heap means no parent is smaller than either child, which
 * is equivalent to checking every non-root element against its parent.
 */
static tk_bool array_is_heap(const int *a, size_t n) {
  for (size_t i = 1; i < n; ++i) {
    if (a[(i - 1) / 2] < a[i])
      return false;
  }
  return true;
}

// --- Fixtures ---

static tk_vec_t *make_int_vec(const int *values, size_t n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = 0; i < n; ++i)
    tk_vec_push_back(v, &values[i]);
  return v;
}

/** Reads the whole vector back into `out` (caller guarantees room). */
static void vec_to_ints(const tk_vec_t *v, int *out, size_t n) {
  for (size_t i = 0; i < n; ++i)
    out[i] = *(const int *)tk_vec_at(v, i);
}

/** Asserts that the vector satisfies the heap property, independently. */
static void assert_vec_is_heap(const tk_vec_t *v) {
  size_t n = tk_vec_size(v);
  int buffer[64];
  cr_assert(n <= 64, "fixture too large for the stack buffer");
  vec_to_ints(v, buffer, n);
  cr_assert(array_is_heap(buffer, n), "range is not a heap");
}

// ===========================================================================
// is_heap: agrees with the independent checker on hand-built fixtures,
// including the zero-, one- and two-element boundaries.
// ===========================================================================

Test(algo_heap, is_heap_empty_range_is_true) {
  tk_vec_t *v = tk_vec_create(sizeof(int));

  cr_assert(tk_algo_is_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp) == true,
            "an empty range is trivially a heap");

  tk_vec_destroy(v);
}

Test(algo_heap, is_heap_single_element_is_true) {
  int values[] = {42};
  tk_vec_t *v = make_int_vec(values, 1);

  cr_assert(tk_algo_is_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp) == true,
            "a one-element range is trivially a heap");

  tk_vec_destroy(v);
}

Test(algo_heap, is_heap_two_element_boundaries) {
  int ordered[] = {2, 1}; // parent above child: a heap
  int reversed[] = {1, 2}; // child above parent: not a heap

  tk_vec_t *heap = make_int_vec(ordered, 2);
  tk_vec_t *not_heap = make_int_vec(reversed, 2);

  cr_assert(tk_algo_is_heap(tk_vec_begin(heap), tk_vec_end(heap), int_cmp) ==
                true,
            "{2, 1} must be a heap");
  cr_assert(tk_algo_is_heap(tk_vec_begin(not_heap), tk_vec_end(not_heap),
                            int_cmp) == false,
            "{1, 2} must not be a heap");

  tk_vec_destroy(heap);
  tk_vec_destroy(not_heap);
}

Test(algo_heap, is_heap_agrees_with_independent_checker) {
  // Two hand-built fixtures of the same shape: one a heap, one with a single
  // violated edge deep in the last level.
  int heap_values[] = {10, 7, 9, 3, 5, 8, 1};
  int broken_values[] = {10, 7, 9, 3, 11, 8, 1}; // index 4 (11) beats index 1 (7)

  cr_assert(array_is_heap(heap_values, 7),
            "fixture bug: the 'heap' fixture must be a heap");

  tk_vec_t *heap = make_int_vec(heap_values, 7);
  tk_vec_t *broken = make_int_vec(broken_values, 7);

  cr_assert(tk_algo_is_heap(tk_vec_begin(heap), tk_vec_end(heap), int_cmp) ==
                true,
            "is_heap must accept a well-formed heap");
  cr_assert(tk_algo_is_heap(tk_vec_begin(broken), tk_vec_end(broken),
                            int_cmp) == false,
            "is_heap must reject a range whose last level violates the "
            "property");

  tk_vec_destroy(heap);
  tk_vec_destroy(broken);
}

Test(algo_heap, is_heap_accepts_equal_elements) {
  // Equal elements never violate a max-heap: the comparator reports 0, which
  // is not "above".
  int values[] = {5, 5, 5, 5, 5};
  tk_vec_t *v = make_int_vec(values, 5);

  cr_assert(tk_algo_is_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp) == true,
            "a range of equal elements is a heap");

  tk_vec_destroy(v);
}

// ===========================================================================
// make_heap: turns an arbitrary range into a heap, in place.
// ===========================================================================

Test(algo_heap, make_heap_satisfies_property) {
  int values[] = {3, 1, 4, 1, 5, 9, 2, 6, 5};
  tk_vec_t *v = make_int_vec(values, 9);

  cr_assert(array_is_heap(values, 9) == false,
            "fixture bug: the input must start out as a non-heap");

  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  assert_vec_is_heap(v);
  cr_assert_eq(*(int *)tk_vec_at(v, 0), 9, "the root must be the maximum");

  tk_vec_destroy(v);
}

Test(algo_heap, make_heap_is_a_permutation_of_the_input) {
  // A heap reorders; it must not lose, duplicate or invent elements.
  int values[] = {5, 3, 8, 1, 9, 2};
  tk_vec_t *v = make_int_vec(values, 6);

  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  int expected[] = {1, 2, 3, 5, 8, 9}; // the input in ascending order
  for (size_t i = 0; i < 6; ++i) {
    size_t found = 0;
    for (size_t j = 0; j < 6; ++j) {
      if (*(int *)tk_vec_at(v, j) == expected[i])
        ++found;
    }
    cr_assert_eq(found, (size_t)1, "value %d must appear exactly once",
                 expected[i]);
  }

  tk_vec_destroy(v);
}

Test(algo_heap, make_heap_empty_and_single_element_are_noops) {
  tk_vec_t *empty = tk_vec_create(sizeof(int));
  int single_value = 7;
  tk_vec_t *single = make_int_vec(&single_value, 1);

  tk_algo_make_heap(tk_vec_begin(empty), tk_vec_end(empty), int_cmp);
  tk_algo_make_heap(tk_vec_begin(single), tk_vec_end(single), int_cmp);

  cr_assert_eq(tk_vec_size(empty), (size_t)0, "empty range must stay empty");
  cr_assert_eq(*(int *)tk_vec_at(single, 0), 7,
               "a single element must be left alone");

  tk_vec_destroy(empty);
  tk_vec_destroy(single);
}

Test(algo_heap, make_heap_two_elements_orders_them) {
  int values[] = {1, 2};
  tk_vec_t *v = make_int_vec(values, 2);

  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  cr_assert_eq(*(int *)tk_vec_at(v, 0), 2, "the larger element must be root");
  cr_assert_eq(*(int *)tk_vec_at(v, 1), 1, "the smaller element must follow");

  tk_vec_destroy(v);
}

// ===========================================================================
// push_heap: the range minus its last element is the existing heap, and the
// last element is sifted up into it.
// ===========================================================================

Test(algo_heap, push_heap_sifts_the_new_last_element_up) {
  int heap_values[] = {10, 7, 9};
  tk_vec_t *v = make_int_vec(heap_values, 3);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  int pushed = 12; // larger than every existing element: must reach the root
  tk_vec_push_back(v, &pushed);
  tk_algo_push_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  cr_assert_eq(*(int *)tk_vec_at(v, 0), 12,
               "the pushed element must sift all the way to the root");
  assert_vec_is_heap(v);

  tk_vec_destroy(v);
}

Test(algo_heap, push_heap_keeps_the_property_for_a_smaller_element) {
  int heap_values[] = {10, 7, 9};
  tk_vec_t *v = make_int_vec(heap_values, 3);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  int pushed = 1; // smaller than everything: stays where it was appended
  tk_vec_push_back(v, &pushed);
  tk_algo_push_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  cr_assert_eq(*(int *)tk_vec_at(v, 0), 10, "the root must be unchanged");
  cr_assert_eq(*(int *)tk_vec_at(v, 3), 1,
               "a smaller element must stay at the end");
  assert_vec_is_heap(v);

  tk_vec_destroy(v);
}

Test(algo_heap, push_heap_maintains_the_property_across_many_pushes) {
  int seed[] = {20, 15, 18, 8, 9, 14, 12};
  tk_vec_t *v = make_int_vec(seed, 7);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  int to_push[] = {25, 3, 17, 30, 1, 11};
  for (size_t i = 0; i < sizeof(to_push) / sizeof(to_push[0]); ++i) {
    tk_vec_push_back(v, &to_push[i]);
    tk_algo_push_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);
    assert_vec_is_heap(v);
  }

  cr_assert_eq(*(int *)tk_vec_at(v, 0), 30,
               "the maximum pushed element must end up at the root");

  tk_vec_destroy(v);
}

Test(algo_heap, push_heap_is_a_noop_below_two_elements) {
  tk_vec_t *empty = tk_vec_create(sizeof(int));
  int single_value = 7;
  tk_vec_t *single = make_int_vec(&single_value, 1);

  tk_algo_push_heap(tk_vec_begin(empty), tk_vec_end(empty), int_cmp);
  tk_algo_push_heap(tk_vec_begin(single), tk_vec_end(single), int_cmp);

  cr_assert_eq(tk_vec_size(empty), (size_t)0, "empty range must stay empty");
  cr_assert_eq(*(int *)tk_vec_at(single, 0), 7,
               "a single element must be left alone");

  tk_vec_destroy(empty);
  tk_vec_destroy(single);
}

// ===========================================================================
// pop_heap: the root lands at the last position, the property is restored over
// the shortened range, and removal is left to the caller.
// ===========================================================================

Test(algo_heap, pop_heap_moves_the_root_to_the_last_position) {
  int values[] = {10, 7, 9, 3, 5, 8};
  tk_vec_t *v = make_int_vec(values, 6);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  tk_algo_pop_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  cr_assert_eq(*(int *)tk_vec_at(v, 5), 10,
               "the old root must land at the last position");

  // The shortened range must still be a heap. Check it independently over the
  // first five elements only.
  int shortened[5];
  vec_to_ints(v, shortened, 5);
  cr_assert(array_is_heap(shortened, 5),
            "the first n-1 elements must satisfy the heap property after pop");

  tk_vec_destroy(v);
}

Test(algo_heap, pop_heap_leaves_removal_to_the_caller) {
  int values[] = {10, 7, 9};
  tk_vec_t *v = make_int_vec(values, 3);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  tk_algo_pop_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  cr_assert_eq(tk_vec_size(v), (size_t)3,
               "pop_heap must not shrink the range; the caller removes");

  tk_vec_destroy(v);
}

Test(algo_heap, pop_heap_empties_the_heap_one_root_at_a_time) {
  // Draining by hand: pop, then remove, then pop again. This is the workflow
  // the function is meant to support.
  int values[] = {5, 1, 8, 3, 9, 2};
  tk_vec_t *v = make_int_vec(values, 6);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  int drained[6];
  size_t drained_count = 0;
  while (tk_vec_size(v) > 0) {
    tk_algo_pop_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);
    drained[drained_count++] = *(int *)tk_vec_back(v);
    tk_vec_pop_back(v);
  }

  int expected[] = {9, 8, 5, 3, 2, 1}; // descending: max first
  for (size_t i = 0; i < 6; ++i) {
    cr_assert_eq(drained[i], expected[i],
                 "drained element %zu must be the next largest", i);
  }

  tk_vec_destroy(v);
}

Test(algo_heap, pop_heap_is_a_noop_below_two_elements) {
  tk_vec_t *empty = tk_vec_create(sizeof(int));
  int single_value = 7;
  tk_vec_t *single = make_int_vec(&single_value, 1);

  tk_algo_pop_heap(tk_vec_begin(empty), tk_vec_end(empty), int_cmp);
  tk_algo_pop_heap(tk_vec_begin(single), tk_vec_end(single), int_cmp);

  cr_assert_eq(tk_vec_size(empty), (size_t)0, "empty range must stay empty");
  cr_assert_eq(*(int *)tk_vec_at(single, 0), 7,
               "a single element must be left alone");

  tk_vec_destroy(empty);
  tk_vec_destroy(single);
}

// ===========================================================================
// sort_heap: drains a heap into ascending order.
// ===========================================================================

Test(algo_heap, sort_heap_drains_into_ascending_order) {
  int values[] = {4, 1, 3, 2, 9, 7};
  tk_vec_t *v = make_int_vec(values, 6);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  tk_algo_sort_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  int expected[] = {1, 2, 3, 4, 7, 9};
  for (size_t i = 0; i < 6; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(v, i), expected[i],
                 "drained range must be ascending at index %zu", i);
  }

  tk_vec_destroy(v);
}

Test(algo_heap, sort_heap_handles_duplicates) {
  int values[] = {3, 3, 1, 2, 2, 3, 1};
  tk_vec_t *v = make_int_vec(values, 7);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  tk_algo_sort_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  int expected[] = {1, 1, 2, 2, 3, 3, 3};
  for (size_t i = 0; i < 7; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(v, i), expected[i],
                 "duplicate-heavy range must be ascending at index %zu", i);
  }

  tk_vec_destroy(v);
}

Test(algo_heap, sort_heap_on_empty_and_single_element_is_a_noop) {
  tk_vec_t *empty = tk_vec_create(sizeof(int));
  int single_value = 7;
  tk_vec_t *single = make_int_vec(&single_value, 1);

  tk_algo_sort_heap(tk_vec_begin(empty), tk_vec_end(empty), int_cmp);
  tk_algo_sort_heap(tk_vec_begin(single), tk_vec_end(single), int_cmp);

  cr_assert_eq(tk_vec_size(empty), (size_t)0, "empty range must stay empty");
  cr_assert_eq(*(int *)tk_vec_at(single, 0), 7,
               "a single element must be left alone");

  tk_vec_destroy(empty);
  tk_vec_destroy(single);
}

// ===========================================================================
// Element size: a 12-byte struct proves the heap reads the size from the
// iterator protocol instead of assuming pointer-sized elements.
// ===========================================================================

typedef struct {
  int key;
  int padding[2]; // makes the element 12 bytes, larger than a pointer
} triple_t;

static int triple_cmp(const void *a, const void *b) {
  int x = ((const triple_t *)a)->key;
  int y = ((const triple_t *)b)->key;
  return (x > y) - (x < y);
}

Test(algo_heap, heap_family_works_on_twelve_byte_structs) {
  cr_assert_eq(sizeof(triple_t), (size_t)12, "triple_t must be 12 bytes");

  int keys[] = {5, 1, 4, 1, 3};
  tk_vec_t *v = tk_vec_create(sizeof(triple_t));
  cr_assert_not_null(v);
  for (size_t i = 0; i < 5; ++i) {
    triple_t t;
    t.key = keys[i];
    t.padding[0] = (int)i; // tags the element so a partial swap is visible
    t.padding[1] = -(int)i;
    tk_vec_push_back(v, &t);
  }

  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), triple_cmp);
  tk_algo_sort_heap(tk_vec_begin(v), tk_vec_end(v), triple_cmp);

  int expected[] = {1, 1, 3, 4, 5};
  for (size_t i = 0; i < 5; ++i) {
    const triple_t *got = (const triple_t *)tk_vec_at(v, i);
    cr_assert_eq(got->key, expected[i], "key at index %zu", i);
    // The padding travelled with its key: a swap that moved only part of the
    // element would break this pairing.
    cr_assert_eq(got->padding[0], -got->padding[1],
                 "element %zu was split: its padding no longer belongs to its "
                 "key",
                 i);
  }

  tk_vec_destroy(v);
}

// ===========================================================================
// The parent issue's coverage matrix for the family: already sorted, reverse
// sorted, all elements equal, and a large deterministic pseudo-random range.
// ===========================================================================

/** Independent heap-property check that reads through tk_vec_at, so it scales
 * past the stack buffer assert_vec_is_heap needs. */
static void assert_vec_is_heap_at_scale(const tk_vec_t *v) {
  const size_t n = tk_vec_size(v);
  for (size_t i = 1; i < n; ++i) {
    int parent = *(const int *)tk_vec_at(v, (i - 1) / 2);
    int child = *(const int *)tk_vec_at(v, i);
    cr_assert(parent >= child, "heap property violated at index %zu", i);
  }
}

/** Asserts the vector reads non-decreasing from front to back. */
static void assert_vec_is_ascending(const tk_vec_t *v) {
  const size_t n = tk_vec_size(v);
  for (size_t i = 1; i < n; ++i) {
    int prev = *(const int *)tk_vec_at(v, i - 1);
    int cur = *(const int *)tk_vec_at(v, i);
    cr_assert(prev <= cur, "range is not ascending at index %zu", i);
  }
}

/** Deterministic pseudo-random source: the test must be reproducible. */
static unsigned next_random(unsigned *state) {
  *state = *state * 1103515245u + 12345u;
  return (*state >> 16) & 0x7FFFu;
}

Test(algo_heap, make_heap_handles_sorted_and_reverse_sorted_input) {
  int ascending[] = {1, 2, 3, 4, 5, 6, 7, 8};
  int descending[] = {8, 7, 6, 5, 4, 3, 2, 1};

  tk_vec_t *sorted = make_int_vec(ascending, 8);
  tk_vec_t *reversed = make_int_vec(descending, 8);

  tk_algo_make_heap(tk_vec_begin(sorted), tk_vec_end(sorted), int_cmp);
  tk_algo_make_heap(tk_vec_begin(reversed), tk_vec_end(reversed), int_cmp);

  assert_vec_is_heap(sorted);
  assert_vec_is_heap(reversed);
  cr_assert_eq(*(int *)tk_vec_at(sorted, 0), 8, "sorted input: root");
  cr_assert_eq(*(int *)tk_vec_at(reversed, 0), 8, "reverse-sorted input: root");

  tk_vec_destroy(sorted);
  tk_vec_destroy(reversed);
}

Test(algo_heap, sort_heap_drains_sorted_and_reverse_sorted_heaps) {
  // Both orders, because a drain of already-sorted data is where an
  // off-by-one in the shrinking boundary shows up.
  int ascending[] = {1, 2, 3, 4, 5, 6, 7, 8};
  int descending[] = {8, 7, 6, 5, 4, 3, 2, 1};

  tk_vec_t *sorted = make_int_vec(ascending, 8);
  tk_vec_t *reversed = make_int_vec(descending, 8);

  tk_algo_make_heap(tk_vec_begin(sorted), tk_vec_end(sorted), int_cmp);
  tk_algo_sort_heap(tk_vec_begin(sorted), tk_vec_end(sorted), int_cmp);
  assert_vec_is_ascending(sorted);

  tk_algo_make_heap(tk_vec_begin(reversed), tk_vec_end(reversed), int_cmp);
  tk_algo_sort_heap(tk_vec_begin(reversed), tk_vec_end(reversed), int_cmp);
  assert_vec_is_ascending(reversed);

  tk_vec_destroy(sorted);
  tk_vec_destroy(reversed);
}

Test(algo_heap, all_equal_elements_survive_make_and_drain) {
  int values[] = {4, 4, 4, 4, 4, 4, 4};
  tk_vec_t *v = make_int_vec(values, 7);

  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);
  assert_vec_is_heap(v);
  cr_assert(tk_algo_is_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp) == true,
            "equal elements never violate the property");

  tk_algo_sort_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  for (size_t i = 0; i < 7; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(v, i), 4,
                 "draining equal elements must not invent or lose one");
  }

  tk_vec_destroy(v);
}

Test(algo_heap, make_heap_handles_one_thousand_elements) {
  enum { n = 1000 };
  int values[n];
  unsigned state = 12345u;
  int maximum = 0;
  for (size_t i = 0; i < n; ++i) {
    values[i] = (int)(next_random(&state) % 1000u);
    if (values[i] > maximum)
      maximum = values[i];
  }

  tk_vec_t *v = make_int_vec(values, n);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  assert_vec_is_heap_at_scale(v);
  cr_assert_eq(*(int *)tk_vec_at(v, 0), maximum,
               "the root must be the maximum of one thousand elements");

  tk_vec_destroy(v);
}

Test(algo_heap, sort_heap_drains_one_thousand_elements) {
  enum { n = 1000 };
  int values[n];
  unsigned state = 999983u; // a different seed from the make_heap test
  for (size_t i = 0; i < n; ++i)
    values[i] = (int)(next_random(&state) % 1000u);

  tk_vec_t *v = make_int_vec(values, n);
  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);
  tk_algo_sort_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  assert_vec_is_ascending(v);

  // Ascending alone would also pass if elements had been lost or duplicated,
  // so check that the multiset survived: the sum is a cheap, exact witness for
  // a range of non-negative ints.
  long expected_sum = 0;
  for (size_t i = 0; i < n; ++i)
    expected_sum += values[i];
  long got_sum = 0;
  for (size_t i = 0; i < n; ++i)
    got_sum += *(int *)tk_vec_at(v, i);
  cr_assert_eq(got_sum, expected_sum,
               "draining must be a permutation of the input");

  tk_vec_destroy(v);
}

// ===========================================================================
// Debug-only: caller violations must trip an assertion rather than produce a
// wrong answer or write through a bad pointer. TK_ASSERT aborts, so each call
// runs in a forked grandchild (same pattern as the vtable-validator tests).
//
// The release half of the contract -- "return immediately and leave the range
// untouched" -- is pinned by test_algo_heap_release.c, which is compiled with
// NDEBUG. It cannot be observed from this file.
// ===========================================================================
#ifndef NDEBUG

// These run in a forked grandchild, where Criterion's assertion machinery is
// not available: failures are reported by _exit(2) instead of cr_assert.
static void sort_heap_on_a_non_heap(void) {
  int values[] = {1, 2, 3}; // ascending: the minimum sits at the root
  tk_vec_t *v = tk_vec_create(sizeof(int));
  if (!v)
    _exit(2);
  for (size_t i = 0; i < 3; ++i)
    tk_vec_push_back(v, &values[i]);

  tk_algo_sort_heap(tk_vec_begin(v), tk_vec_end(v), int_cmp);
}

/** Calls one heap entry point over a three-element list. */
static void heap_entry_on_a_list(void (*entry)(tk_iterator_t, tk_iterator_t,
                                               tk_comparator_t)) {
  int values[] = {3, 1, 2};
  tk_list_t *l = tk_list_create(sizeof(int));
  if (!l)
    _exit(2);
  for (size_t i = 0; i < 3; ++i)
    tk_list_push_back(l, &values[i]);

  entry(tk_list_begin(l), tk_list_end(l), int_cmp);
}

static void make_heap_on_a_list(void) { heap_entry_on_a_list(tk_algo_make_heap); }

static void push_heap_on_a_list(void) { heap_entry_on_a_list(tk_algo_push_heap); }

static void pop_heap_on_a_list(void) { heap_entry_on_a_list(tk_algo_pop_heap); }

static void sort_heap_on_a_list(void) { heap_entry_on_a_list(tk_algo_sort_heap); }

static void is_heap_on_a_list(void) {
  int values[] = {3, 1, 2};
  tk_list_t *l = tk_list_create(sizeof(int));
  if (!l)
    _exit(2);
  for (size_t i = 0; i < 3; ++i)
    tk_list_push_back(l, &values[i]);

  (void)tk_algo_is_heap(tk_list_begin(l), tk_list_end(l), int_cmp);
}

static void null_comparator(void) {
  int values[] = {3, 1, 2};
  tk_vec_t *v = tk_vec_create(sizeof(int));
  if (!v)
    _exit(2);
  for (size_t i = 0; i < 3; ++i)
    tk_vec_push_back(v, &values[i]);

  tk_algo_make_heap(tk_vec_begin(v), tk_vec_end(v), NULL);
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

Test(algo_heap, sort_heap_on_a_non_heap_asserts) {
  cr_assert(aborts(sort_heap_on_a_non_heap),
            "draining a non-heap must assert instead of yielding garbage");
}

Test(algo_heap, make_heap_rejects_a_bidirectional_range) {
  cr_assert(aborts(make_heap_on_a_list),
            "make_heap on a list must assert: the family requires random "
            "access");
}

Test(algo_heap, push_heap_rejects_a_bidirectional_range) {
  cr_assert(aborts(push_heap_on_a_list),
            "push_heap on a list must assert");
}

Test(algo_heap, pop_heap_rejects_a_bidirectional_range) {
  cr_assert(aborts(pop_heap_on_a_list),
            "pop_heap on a list must assert");
}

Test(algo_heap, sort_heap_rejects_a_bidirectional_range) {
  cr_assert(aborts(sort_heap_on_a_list),
            "sort_heap on a list must assert");
}

Test(algo_heap, is_heap_rejects_a_bidirectional_range) {
  cr_assert(aborts(is_heap_on_a_list), "is_heap on a list must assert");
}

Test(algo_heap, null_comparator_asserts) {
  cr_assert(aborts(null_comparator), "a NULL comparator must assert");
}

#endif // !NDEBUG
