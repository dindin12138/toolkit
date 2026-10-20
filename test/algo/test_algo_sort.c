/**
 * @file test_algo_sort.c
 * @brief Tests for tk_algo_sort (issue #4): in-place introsort over a
 * random-access range.
 *
 * Every assertion goes through the public header, driving iterators obtained
 * from a real container. Nothing here inspects how the sort partitions or how
 * deep it recurses: only the arrangement of the range afterwards, plus a few
 * comparator-call budgets where the point is "this input is not quadratic".
 *
 * The expected order is never produced by calling the library again -- it comes
 * from the C standard library's qsort, from a hand-written literal, or from a
 * property (ascending, a permutation) checked directly over the elements.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include <signal.h>   // SIGABRT
#include <stddef.h>   // size_t
#include <stdlib.h>   // qsort, malloc, free
#include <string.h>   // memcpy
#include <sys/wait.h> // waitpid, WIFSIGNALED, WTERMSIG
#include <unistd.h>   // fork, _exit, pid_t

#include <tk/algo/sort.h>
#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

// --- Comparators ---

/** Three-way ascending comparator for int elements (qsort convention). */
static int int_cmp(const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return (x > y) - (x < y);
}

/** Three-way descending comparator: the sort must follow cmp, not the values. */
static int int_cmp_descending(const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return (y > x) - (y < x);
}

// --- Fixtures and independent checkers ---

static tk_vec_t *make_int_vec(const int *values, size_t n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = 0; i < n; ++i)
    tk_vec_push_back(v, &values[i]);
  return v;
}

/** Asserts the vector reads non-decreasing from front to back. */
static void assert_vec_is_ascending(const tk_vec_t *v) {
  const size_t n = tk_vec_size(v);
  for (size_t i = 1; i < n; ++i) {
    int prev = *(const int *)tk_vec_at(v, i - 1);
    int cur = *(const int *)tk_vec_at(v, i);
    cr_assert(prev <= cur, "range is not ascending at index %zu (%d > %d)", i,
              prev, cur);
  }
}

/** Asserts the vector still holds exactly the multiset `expected` (sorted). */
static void assert_vec_holds(const tk_vec_t *v, const int *expected_sorted,
                             size_t n) {
  cr_assert_eq(tk_vec_size(v), n, "the sort must not change the range size");
  for (size_t i = 0; i < n; ++i) {
    int got = *(const int *)tk_vec_at(v, i);
    cr_assert_eq(got, expected_sorted[i], "element %zu", i);
  }
}

/** Deterministic pseudo-random source: the test must be reproducible. */
static unsigned next_random(unsigned *state) {
  *state = *state * 1103515245u + 12345u;
  return (*state >> 16) & 0x7FFFu;
}

// ===========================================================================
// The boundaries and the shapes the issue names: empty, one, two, already
// sorted, reverse sorted, all equal, many duplicates, and a random range of
// one thousand elements.
// ===========================================================================

Test(algo_sort, empty_range_stays_empty) {
  tk_vec_t *v = tk_vec_create(sizeof(int));

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  cr_assert_eq(tk_vec_size(v), (size_t)0, "an empty range must stay empty");

  tk_vec_destroy(v);
}

Test(algo_sort, single_element_stays_put) {
  int values[] = {42};
  tk_vec_t *v = make_int_vec(values, 1);

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  assert_vec_holds(v, values, 1);

  tk_vec_destroy(v);
}

Test(algo_sort, two_elements_are_ordered_in_either_input_order) {
  int ascending[] = {1, 2};
  int descending[] = {2, 1};

  tk_vec_t *a = make_int_vec(ascending, 2);
  tk_vec_t *d = make_int_vec(descending, 2);

  tk_algo_sort(tk_vec_begin(a), tk_vec_end(a), int_cmp);
  tk_algo_sort(tk_vec_begin(d), tk_vec_end(d), int_cmp);

  int expected[] = {1, 2};
  assert_vec_holds(a, expected, 2);
  assert_vec_holds(d, expected, 2);

  tk_vec_destroy(a);
  tk_vec_destroy(d);
}

Test(algo_sort, already_sorted_range_is_left_in_order) {
  // Larger than the insertion-sort threshold on purpose: a range of eight
  // elements would be finished by the closing insertion-sort pass without the
  // partition ever running, and the pivot choice would go untested here.
  enum { n = 1000 };
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = 0; i < n; ++i) {
    int value = (int)i;
    tk_vec_push_back(v, &value);
  }

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  assert_vec_is_ascending(v);
  for (size_t i = 0; i < n; ++i)
    cr_assert_eq(*(int *)tk_vec_at(v, i), (int)i,
                 "sorted input must come back untouched at index %zu", i);

  tk_vec_destroy(v);
}

Test(algo_sort, reverse_sorted_range_is_ordered) {
  // The shape that makes a first-element pivot quadratic: see the comment on
  // the previous test for why it has to exceed the threshold.
  enum { n = 1000 };
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = n; i > 0; --i) {
    int value = (int)i;
    tk_vec_push_back(v, &value);
  }

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  assert_vec_is_ascending(v);
  for (size_t i = 0; i < n; ++i)
    cr_assert_eq(*(int *)tk_vec_at(v, i), (int)(i + 1),
                 "reverse-sorted input must come out ascending at index %zu",
                 i);

  tk_vec_destroy(v);
}

Test(algo_sort, all_equal_elements_survive) {
  int values[] = {4, 4, 4, 4, 4, 4, 4};
  tk_vec_t *v = make_int_vec(values, 7);

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  assert_vec_holds(v, values, 7);

  tk_vec_destroy(v);
}

Test(algo_sort, duplicate_heavy_range_is_ordered) {
  int values[] = {3, 1, 3, 2, 3, 1, 2, 3, 3, 1, 2, 2, 3, 1, 1, 2, 3};
  tk_vec_t *v = make_int_vec(values, 17);

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  int expected[] = {1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3};
  assert_vec_holds(v, expected, 17);

  tk_vec_destroy(v);
}

Test(algo_sort, orders_one_thousand_random_elements) {
  enum { n = 1000 };
  int values[n];
  unsigned state = 12345u;
  for (size_t i = 0; i < n; ++i)
    values[i] = (int)(next_random(&state) % 1000u);

  tk_vec_t *v = make_int_vec(values, n);
  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp);

  assert_vec_is_ascending(v);

  // Ascending alone would also pass if elements had been lost or duplicated,
  // so check that the multiset survived. The sum is an exact witness for a
  // range of non-negative ints.
  long expected_sum = 0;
  for (size_t i = 0; i < n; ++i)
    expected_sum += values[i];
  long got_sum = 0;
  for (size_t i = 0; i < n; ++i)
    got_sum += *(int *)tk_vec_at(v, i);
  cr_assert_eq(got_sum, expected_sum,
               "sorting must be a permutation of the input");

  tk_vec_destroy(v);
}

Test(algo_sort, honours_a_descending_comparator) {
  int values[] = {5, 1, 9, 3, 7};
  tk_vec_t *v = make_int_vec(values, 5);

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp_descending);

  int expected[] = {9, 7, 5, 3, 1};
  assert_vec_holds(v, expected, 5);

  tk_vec_destroy(v);
}

// ===========================================================================
// The reference: the same input, sorted by the C standard library's qsort,
// must come out element-for-element identical. qsort is an independent
// implementation of the same contract, so it cannot share a mistake with this
// one.
// ===========================================================================

Test(algo_sort, agrees_with_qsort_element_for_element) {
  // Sizes straddle the boundaries the implementation cares about: below the
  // insertion-sort threshold, just above it, and large enough to recurse
  // several levels. A narrow value range keeps duplicates in the mix.
  const size_t sizes[] = {0, 1, 2, 3, 5, 16, 17, 64, 257, 1000};
  const size_t size_count = sizeof(sizes) / sizeof(sizes[0]);

  for (size_t s = 0; s < size_count; ++s) {
    const size_t n = sizes[s];
    int *input = NULL;
    if (n > 0) {
      input = malloc(n * sizeof(int));
      cr_assert_not_null(input);
    }
    unsigned state = 7919u + (unsigned)n; // a different seed per size
    for (size_t i = 0; i < n; ++i)
      input[i] = (int)(next_random(&state) % 50u);

    int *reference = NULL;
    if (n > 0) {
      reference = malloc(n * sizeof(int));
      cr_assert_not_null(reference);
      memcpy(reference, input, n * sizeof(int));
      qsort(reference, n, sizeof(int), int_cmp);
    }

    tk_vec_t *v = make_int_vec(input, n);
    tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), int_cmp);

    cr_assert_eq(tk_vec_size(v), n, "size %zu: the range size must not change",
                 n);
    for (size_t i = 0; i < n; ++i) {
      cr_assert_eq(*(int *)tk_vec_at(v, i), reference[i],
                   "size %zu: element %zu differs from qsort's answer", n, i);
    }

    tk_vec_destroy(v);
    free(reference);
    free(input);
  }
}

// ===========================================================================
// Element size: a 12-byte struct proves the sort reads the size from the
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

Test(algo_sort, sorts_twelve_byte_structs) {
  cr_assert_eq(sizeof(triple_t), (size_t)12, "triple_t must be 12 bytes");

  enum { n = 40 }; // enough elements to exercise the partition, not just the
                   // insertion-sort tail
  tk_vec_t *v = tk_vec_create(sizeof(triple_t));
  cr_assert_not_null(v);

  int keys[n];
  unsigned state = 424242u;
  for (size_t i = 0; i < n; ++i) {
    triple_t t;
    t.key = (int)(next_random(&state) % 20u); // duplicates on purpose
    keys[i] = t.key;
    t.padding[0] = (int)i; // tags the element so a partial swap is visible
    t.padding[1] = -(int)i;
    tk_vec_push_back(v, &t);
  }

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), triple_cmp);

  for (size_t i = 1; i < n; ++i) {
    int prev = ((const triple_t *)tk_vec_at(v, i - 1))->key;
    int cur = ((const triple_t *)tk_vec_at(v, i))->key;
    cr_assert(prev <= cur, "struct range is not ascending at index %zu", i);
  }
  for (size_t i = 0; i < n; ++i) {
    const triple_t *got = (const triple_t *)tk_vec_at(v, i);
    // The padding travelled with its key: a swap that moved only part of the
    // element would break this pairing, and a sort that assumed 8-byte
    // elements would corrupt the range outright.
    cr_assert_eq(got->padding[0], -got->padding[1],
                 "element %zu was split: its padding no longer belongs to its "
                 "key",
                 i);
  }

  tk_vec_destroy(v);
}

// ===========================================================================
// Cost: the guarantee is O(n log n) on adversarial input and linear on equal
// elements. Both are measured in comparator calls, which is the only cost the
// caller can observe.
// ===========================================================================

/** Comparator calls made so far. Criterion runs each test in its own process,
 * so this cannot leak between tests. */
static size_t g_comparisons = 0;

static int counting_cmp(const void *a, const void *b) {
  ++g_comparisons;
  return int_cmp(a, b);
}

/** floor(log2(n)), the yardstick the budgets below are expressed in. */
static size_t floor_log2(size_t n) {
  size_t levels = 0;
  while (n > 1) {
    n >>= 1;
    ++levels;
  }
  return levels;
}

/**
 * @brief Builds a median-of-three killer: a "mountain" that rises to the middle
 * and falls back.
 *
 * On this shape the median of the first, middle and last elements is the middle
 * element of the range at every level, so the pivot sits as far from the centre
 * as the range allows and the partition is maximally unbalanced. Plain
 * quicksort therefore recurses about n/2 levels deep and runs in quadratic
 * time; introsort must stay logarithmic and O(n log n). This is the input the
 * acceptance criterion asks for.
 */
static tk_vec_t *make_median_of_three_killer(size_t n) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);

  const size_t half = n / 2;
  for (size_t i = 0; i < half; ++i) {
    int value = (int)(i + 1); // rising half
    tk_vec_push_back(v, &value);
  }
  for (size_t i = n - half; i > 0; --i) {
    int value = (int)i; // falling half
    tk_vec_push_back(v, &value);
  }
  return v;
}

Test(algo_sort, sorts_a_median_of_three_killer_without_blowing_up) {
  const size_t n = 100000;

  tk_vec_t *v = make_median_of_three_killer(n);
  cr_assert_eq(tk_vec_size(v), n, "fixture must hold %zu elements", n);

  g_comparisons = 0;
  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), counting_cmp);

  assert_vec_is_ascending(v);

  // A quadratic sort would need on the order of n^2 / 2 comparisons here --
  // roughly 5e9, three hundred times this budget. Ten times n log2(n) leaves
  // room for the heapsort fallback plus the insertion-sort pass, and nothing
  // else.
  const size_t budget = 10u * n * floor_log2(n);
  cr_assert(g_comparisons <= budget,
            "a median-of-three killer must stay O(n log n): %zu comparisons "
            "for %zu elements exceeds the budget of %zu",
            g_comparisons, n, budget);

  tk_vec_destroy(v);
}

Test(algo_sort, all_equal_range_is_linear) {
  // A range of equal elements is where a partition that cannot recognise
  // equality degrades to quadratic: every element is "not less than" the pivot,
  // so a two-way partition peels one element off per level. The three-way
  // partition must instead finish this in one pass, plus the insertion-sort
  // tail that finds nothing to move.
  const size_t n = 100000;
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = 0; i < n; ++i) {
    int value = 7;
    tk_vec_push_back(v, &value);
  }

  g_comparisons = 0;
  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), counting_cmp);

  for (size_t i = 0; i < n; ++i)
    cr_assert_eq(*(int *)tk_vec_at(v, i), 7, "element %zu was lost", i);

  // One pass of the partition plus one pass of the insertion sort: a small
  // multiple of n, and far below what even a single extra partition level
  // would cost.
  const size_t budget = 10u * n;
  cr_assert(g_comparisons <= budget,
            "an all-equal range must cost linear comparisons: %zu for %zu "
            "elements exceeds the budget of %zu",
            g_comparisons, n, budget);

  tk_vec_destroy(v);
}

Test(algo_sort, duplicate_heavy_range_is_not_quadratic) {
  const size_t n = 100000;
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  unsigned state = 2024u;
  for (size_t i = 0; i < n; ++i) {
    int value = (int)(next_random(&state) % 8u); // only eight distinct values
    tk_vec_push_back(v, &value);
  }

  g_comparisons = 0;
  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), counting_cmp);

  assert_vec_is_ascending(v);

  const size_t budget = 10u * n * floor_log2(n);
  cr_assert(g_comparisons <= budget,
            "a duplicate-heavy range must not be quadratic: %zu comparisons "
            "for %zu elements exceeds the budget of %zu",
            g_comparisons, n, budget);

  tk_vec_destroy(v);
}

// ===========================================================================
// Sorted and reverse-sorted input at scale: the shapes the issue says must not
// degrade. Both are ordered here, so the budget only means something if the
// cost is measured too -- a first-element pivot would be quadratic on both.
// ===========================================================================

/** Fills `v` with 0..n-1, or with n-1..0 when `descending`. */
static tk_vec_t *make_run(size_t n, tk_bool descending) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (size_t i = 0; i < n; ++i) {
    int value = descending ? (int)(n - i) : (int)i;
    tk_vec_push_back(v, &value);
  }
  return v;
}

Test(algo_sort, sorted_and_reverse_sorted_inputs_stay_n_log_n) {
  const size_t n = 100000;
  const size_t budget = 10u * n * floor_log2(n);

  tk_vec_t *ascending = make_run(n, false);
  g_comparisons = 0;
  tk_algo_sort(tk_vec_begin(ascending), tk_vec_end(ascending), counting_cmp);
  assert_vec_is_ascending(ascending);
  cr_assert(g_comparisons <= budget,
            "already sorted input must not degrade: %zu comparisons for %zu "
            "elements exceeds the budget of %zu",
            g_comparisons, n, budget);

  tk_vec_t *descending = make_run(n, true);
  g_comparisons = 0;
  tk_algo_sort(tk_vec_begin(descending), tk_vec_end(descending), counting_cmp);
  assert_vec_is_ascending(descending);
  cr_assert(g_comparisons <= budget,
            "reverse-sorted input must not degrade: %zu comparisons for %zu "
            "elements exceeds the budget of %zu",
            g_comparisons, n, budget);

  tk_vec_destroy(ascending);
  tk_vec_destroy(descending);
}

// ===========================================================================
// Debug-only: a caller violation must trip an assertion rather than produce a
// wrong answer or write through a bad pointer. TK_ASSERT aborts, so each call
// runs in a forked grandchild (same pattern as the heap family's tests).
//
// The release half of the contract -- "return immediately and leave the range
// untouched" -- is pinned by test_algo_sort_release.c, which is compiled with
// NDEBUG. It cannot be observed from this file.
// ===========================================================================
#ifndef NDEBUG

// These run in a forked grandchild, where Criterion's assertion machinery is
// not available: failures are reported by _exit(2) instead of cr_assert.
static void sort_on_a_bidirectional_range(void) {
  int values[] = {3, 1, 2}; // any write would be visible: the list is unsorted
  tk_list_t *l = tk_list_create(sizeof(int));
  if (!l)
    _exit(2);
  for (size_t i = 0; i < 3; ++i)
    tk_list_push_back(l, &values[i]);

  tk_algo_sort(tk_list_begin(l), tk_list_end(l), int_cmp);
}

static void sort_with_a_null_comparator(void) {
  int values[] = {3, 1, 2};
  tk_vec_t *v = tk_vec_create(sizeof(int));
  if (!v)
    _exit(2);
  for (size_t i = 0; i < 3; ++i)
    tk_vec_push_back(v, &values[i]);

  tk_algo_sort(tk_vec_begin(v), tk_vec_end(v), NULL);
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

Test(algo_sort, rejects_a_bidirectional_range) {
  cr_assert(aborts(sort_on_a_bidirectional_range),
            "sorting a list must assert: the sort requires random access");
}

Test(algo_sort, rejects_a_null_comparator) {
  cr_assert(aborts(sort_with_a_null_comparator),
            "a NULL comparator must assert");
}

#endif // !NDEBUG
