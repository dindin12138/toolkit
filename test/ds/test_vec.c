/**
 * @file test_vec.c
 * @brief Unit tests for the tk_vec module using the Criterion framework.
 *
 * This file demonstrates modern C unit testing practices with Criterion,
 * including test suites, fixtures for setup/teardown, and rich assertions.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h> // Modern assertion macros (eq, ne, etc.)
#include <stdint.h>               // For SIZE_MAX
#include <stdio.h>
#include <string.h> // For strcmp in struct test
#include <tk/core/iterator.h>
#include <tk/ds/vec.h>

// --- Test Fixture for Integer Vector ---

static tk_vec_t *vec;

void setup(void) {
  vec = tk_vec_create(sizeof(int));
  cr_assert_not_null(vec, "Vector creation failed in setup");
}

void teardown(void) { tk_vec_destroy(vec); }

TestSuite(vec_suite, .init = setup, .fini = teardown);

// --- Test Cases for Integer Vector Suite ---

Test(vec_suite, lifecycle) {
  cr_assert_eq(tk_vec_size(vec), 0, "Initial size should be 0");
  cr_assert(tk_vec_is_empty(vec), "Vector should be empty initially");
  cr_assert_eq(tk_vec_capacity(vec), 0, "Initial capacity should be 0");
}

Test(vec_suite, push_and_pop) {
  int val1 = 10, val2 = 20;

  tk_vec_push_back(vec, &val1);
  cr_assert_eq(tk_vec_size(vec), 1);
  cr_assert_eq(*(int *)tk_vec_at(vec, 0), 10);

  tk_vec_push_back(vec, &val2);
  cr_assert_eq(tk_vec_size(vec), 2);
  cr_assert_eq(*(int *)tk_vec_back(vec), 20);

  tk_vec_pop_back(vec);
  cr_assert_eq(tk_vec_size(vec), 1);
  cr_assert_eq(*(int *)tk_vec_back(vec), 10);
}

// --- P1-2: const-correct element access (read variants + _mut variants) ---

/**
 * @brief The mutable accessors must return writable pointers to the correct
 * element; the values written are then verified through the read accessors.
 */
Test(vec_suite, access_mut_variants_write) {
  int a = 1, b = 2, c = 3;
  tk_vec_push_back(vec, &a); // index 0
  tk_vec_push_back(vec, &b); // index 1
  tk_vec_push_back(vec, &c); // index 2

  // Write through the mutable variants.
  *(int *)tk_vec_front_mut(vec) = 111; // first
  *(int *)tk_vec_at_mut(vec, 1) = 100; // middle
  *(int *)tk_vec_back_mut(vec) = 222;  // last

  // Read back through the read-only variants and assert.
  cr_assert_eq(*(const int *)tk_vec_at(vec, 0), 111);
  cr_assert_eq(*(const int *)tk_vec_at(vec, 1), 100);
  cr_assert_eq(*(const int *)tk_vec_at(vec, 2), 222);
  cr_assert_eq(*(const int *)tk_vec_front(vec), 111);
  cr_assert_eq(*(const int *)tk_vec_back(vec), 222);
}

/**
 * @brief The read accessors must accept a `const tk_vec_t *` handle and return
 * the correct values.
 */
Test(vec_suite, access_const_variants_on_const_handle) {
  int a = 10, b = 20, c = 30;
  tk_vec_push_back(vec, &a);
  tk_vec_push_back(vec, &b);
  tk_vec_push_back(vec, &c);

  const tk_vec_t *cv = vec; // read-only view of the same vector

  cr_assert_eq(*(const int *)tk_vec_at(cv, 0), 10);
  cr_assert_eq(*(const int *)tk_vec_at(cv, 2), 30);
  cr_assert_eq(*(const int *)tk_vec_front(cv), 10);
  cr_assert_eq(*(const int *)tk_vec_back(cv), 30);
}

/**
 * @brief The `_mut` accessors share the read accessors' sentinel behaviour:
 * a NULL handle, an empty vector, or an out-of-bounds index yields NULL.
 */
Test(vec_suite, access_mut_null_and_bounds) {
  // NULL handle.
  cr_assert_null(tk_vec_at_mut(NULL, 0));
  cr_assert_null(tk_vec_front_mut(NULL));
  cr_assert_null(tk_vec_back_mut(NULL));

  // Empty vector.
  cr_assert_null(tk_vec_at_mut(vec, 0));
  cr_assert_null(tk_vec_front_mut(vec));
  cr_assert_null(tk_vec_back_mut(vec));

  // One element: index 0 is valid, everything else is out of bounds.
  int v = 7;
  tk_vec_push_back(vec, &v);
  cr_assert_not_null(tk_vec_at_mut(vec, 0));
  cr_assert_not_null(tk_vec_front_mut(vec));
  cr_assert_not_null(tk_vec_back_mut(vec));
  cr_assert_null(tk_vec_at_mut(vec, 1), "at_mut(size) must be out of bounds");
  cr_assert_null(tk_vec_at_mut(vec, 100),
                 "at_mut(large_index) must be out of bounds");
}

/**
 * @brief Compile-time signature pins for the six P1-2 accessors.
 *
 * Each initializer below pins one accessor's *exact* function type. Function
 * pointer assignment requires the whole function type to match verbatim
 * (every parameter type AND the return type), so any drift in any of the six
 * signatures is an incompatible-function-pointer-types diagnostic:
 *   - if a read accessor (tk_vec_at / tk_vec_front / tk_vec_back) stopped
 *     returning `const void *` (e.g. regressed to `void *`);
 *   - if a read accessor stopped taking a `const tk_vec_t *` (i.e. its
 *     *parameter* regressed to a non-const `tk_vec_t *`); or
 *   - if a mutable accessor (tk_vec_at_mut / tk_vec_front_mut /
 *     tk_vec_back_mut) stopped taking a non-const `tk_vec_t *` (e.g. started
 *     taking `const tk_vec_t *`),
 * this test target would fail to compile.
 *
 * This is what makes the const-correctness contract *enforced* rather than
 * review-only: assigning the result of a read accessor to a `const void *`
 * local (the previous approach) proves nothing, because `void *` -> `const
 * void *` is a legal implicit conversion. Because a function pointer pins the
 * whole function type, these six lines cover the read accessors' *parameters*
 * (which must stay `const tk_vec_t *`) as well as their return types.
 *
 * Guarantee and its boundary: this project's *test targets* compile with
 * `-Werror=incompatible-function-pointer-types` (see CMakeLists.txt) and Clang
 * diagnoses the mismatch by default too, so the pins hold in both the default
 * and the hardened build. Note, however, that this is a *default-error but
 * suppressible diagnostic*, NOT a language-level constraint -- C99 offers no
 * `_Static_assert` / `_Generic` to pin a signature at compile time. A build
 * that passed `-Wno-incompatible-function-pointer-types` (or otherwise
 * suppressed the diagnostic) could therefore compile a regressed signature
 * silently. This project adds no such flag, so the guarantee holds in
 * practice; the narrow `-Werror=` above exists precisely so that "error by
 * default" no longer depends on a particular compiler version's default
 * behaviour.
 *
 * The six calls at the end consume the pins so the compiler cannot report
 * them as unused; they also double as a runtime sanity check.
 */
Test(vec_suite, access_exact_signature_pins) {
  int v = 5;
  tk_vec_push_back(vec, &v); // exactly one element, at index 0

  const tk_vec_t *cv = vec;

  // --- The six signature pins (compile-time) ---
  const void *(*const p_at)(const tk_vec_t *, size_t) = tk_vec_at;
  void *(*const p_at_mut)(tk_vec_t *, size_t) = tk_vec_at_mut;
  const void *(*const p_front)(const tk_vec_t *) = tk_vec_front;
  void *(*const p_front_mut)(tk_vec_t *) = tk_vec_front_mut;
  const void *(*const p_back)(const tk_vec_t *) = tk_vec_back;
  void *(*const p_back_mut)(tk_vec_t *) = tk_vec_back_mut;

  // --- Consume the pins (also a one-element runtime sanity check) ---
  cr_assert_eq(*(const int *)p_at(cv, 0), 5);
  cr_assert_eq(*(const int *)p_front(cv), 5);
  cr_assert_eq(*(const int *)p_back(cv), 5);
  cr_assert_eq(*(int *)p_at_mut(vec, 0), 5);
  cr_assert_eq(*(int *)p_front_mut(vec), 5);
  cr_assert_eq(*(int *)p_back_mut(vec), 5);
}

Test(vec_suite, reallocation) {
  int num_elements = 1000;
  for (int i = 0; i < num_elements; ++i) {
    tk_vec_push_back(vec, &i);
  }
  cr_assert_eq(tk_vec_size(vec), (size_t)num_elements,
               "Size should be %d after insertions", num_elements);
  for (int i = 0; i < num_elements; ++i) {
    cr_assert_eq(*(int *)tk_vec_at(vec, i), i,
                 "Element at index %d is incorrect", i);
  }
}

Test(vec_suite, capacity_and_clear) {
  tk_vec_reserve(vec, 100);
  cr_assert_geq(tk_vec_capacity(vec), 100, "Capacity should be at least 100");
  cr_assert_eq(tk_vec_size(vec), 0, "Reserve should not change the size");

  for (int i = 0; i < 50; ++i) {
    tk_vec_push_back(vec, &i);
  }
  cr_assert_eq(tk_vec_size(vec), 50);

  tk_vec_clear(vec);
  cr_assert_eq(tk_vec_size(vec), 0, "Size should be 0 after clear");
  cr_assert(tk_vec_is_empty(vec), "Vector should be empty after clear");
  cr_assert_geq(tk_vec_capacity(vec), 100,
                "Capacity should not change after clear");
}

Test(vec_suite, reserve_edge_cases) {
  tk_vec_reserve(vec, 10);
  cr_assert_geq(tk_vec_capacity(vec), 10);
  for (int i = 0; i < 5; ++i) {
    tk_vec_push_back(vec, &i);
  }
  cr_assert_eq(tk_vec_size(vec), 5);
  size_t old_capacity = tk_vec_capacity(vec);

  tk_vec_reserve(vec, 8);
  cr_assert_geq(tk_vec_capacity(vec), 8, "Capacity should still be sufficient");

  old_capacity = tk_vec_capacity(vec);
  tk_vec_reserve(vec, 3);
  cr_assert_eq(tk_vec_size(vec), 5,
               "Size should not change when reserving less than size");
  cr_assert_eq(tk_vec_capacity(vec), old_capacity,
               "Capacity should not shrink below size");

  tk_vec_reserve(vec, 0);
  cr_assert_eq(tk_vec_size(vec), 5, "Size should not change when reserving 0");
}

/**
 * @brief Regression test for issue #9 (NULL-safety) and #12 (overflow).
 *
 * All public tk_vec_* functions must be safe to call with a NULL handle: query
 * functions return a sentinel (0 / true / NULL) and mutating functions return
 * TK_E_INVALID_ARG (or are a no-op), instead of dereferencing NULL.
 */
Test(vec_suite, null_safety) {
  cr_assert_eq(tk_vec_size(NULL), 0, "size(NULL) should be 0");
  cr_assert_eq(tk_vec_capacity(NULL), 0, "capacity(NULL) should be 0");
  cr_assert(tk_vec_is_empty(NULL), "is_empty(NULL) should be true");
  cr_assert_null(tk_vec_at(NULL, 0), "at(NULL, 0) should be NULL");
  cr_assert_null(tk_vec_front(NULL), "front(NULL) should be NULL");
  cr_assert_null(tk_vec_back(NULL), "back(NULL) should be NULL");

  int value = 7;
  cr_assert_eq(tk_vec_reserve(NULL, 10), TK_E_INVALID_ARG,
               "reserve(NULL, n) should return TK_E_INVALID_ARG");
  cr_assert_eq(tk_vec_push_back(NULL, &value), TK_E_INVALID_ARG,
               "push_back(NULL, elem) should return TK_E_INVALID_ARG");
  cr_assert_eq(tk_vec_push_back(vec, NULL), TK_E_INVALID_ARG,
               "push_back(vec, NULL) should return TK_E_INVALID_ARG");

  // These must not crash.
  tk_vec_pop_back(NULL);
  tk_vec_clear(NULL);
  tk_vec_destroy(NULL);
  tk_vec_destroy_full(NULL, NULL);

  cr_assert(true, "Reached the end of the NULL-safety test without crashing");
}

/**
 * @brief Regression test for issue #12: reserve must detect size_t overflow.
 *
 * With element_size > 1, reserving SIZE_MAX elements would overflow
 * `n * element_size`; the implementation must return TK_E_NOMEM up front
 * instead of wrapping around.
 */
Test(vec_suite, reserve_overflow) {
  cr_assert_gt(sizeof(int), 1, "This test assumes element_size > 1");
  tk_error_t err = tk_vec_reserve(vec, SIZE_MAX);
  cr_assert_eq(err, TK_E_NOMEM,
               "reserve(SIZE_MAX) must report TK_E_NOMEM (overflow guard)");
}

/**
 * @brief This test validates the entire iterator protocol implementation
 * for tk_vec_t.
 *
 * It checks:
 * 1. `begin()` and `end()` on an empty vector.
 * 2. `begin()`, `end()`, `next()`, `get()`, and `equal()` on a populated
 * vector.
 * 3. `clone()` and its independence from the original iterator.
 */
Test(vec_suite, iterators) {
  // 1. Test empty vector
  // The 'vec' is empty right after setup()
  tk_iterator_t begin_empty = tk_vec_begin(vec);
  tk_iterator_t end_empty = tk_vec_end(vec);
  cr_assert(tk_iter_equal(&begin_empty, &end_empty),
            "begin() and end() should be equal on an empty vector");

  // 2. Test populated vector
  int v1 = 10, v2 = 20, v3 = 30;
  tk_vec_push_back(vec, &v1);
  tk_vec_push_back(vec, &v2);
  tk_vec_push_back(vec, &v3);

  int expected_values[] = {10, 20, 30};
  int i = 0;

  tk_iterator_t it = tk_vec_begin(vec);
  tk_iterator_t end = tk_vec_end(vec);

  // Assert begin and end are NOT equal
  cr_assert(tk_iter_equal(&it, &end) == false,
            "begin() and end() should not be equal on a populated vector");

  // 3. Test tk_iter_clone()
  tk_iterator_t clone_it;
  tk_iter_clone(&clone_it, &it);
  cr_assert(tk_iter_equal(&clone_it, &it), "Cloned iterator should be equal to "
                                           "its source");

  // Advance the original iterator
  tk_iter_next(&it);
  // They should no longer be equal
  cr_assert(tk_iter_equal(&clone_it, &it) == false,
            "Cloned iterator should be independent of its source after "
            "advancing");
  // The clone should still point to the first element
  cr_assert_eq(*(int *)tk_iter_get(&clone_it), 10,
               "Cloned iterator did not retain the correct position");

  // Reset 'it' to begin for the main loop test
  it = tk_vec_begin(vec);

  // 4. Test the core iteration loop (next, get, equal)
  while (!tk_iter_equal(&it, &end)) {
    // Check against out-of-bounds loop
    cr_assert_lt(i, 3, "Iterator loop ran too many times");

    // Test tk_iter_get()
    void *data = tk_iter_get(&it);
    cr_assert_not_null(data, "tk_iter_get() returned NULL");
    cr_assert_eq(*(int *)data, expected_values[i],
                 "Iterator value mismatch at index %d", i);

    // Test tk_iter_next()
    tk_iter_next(&it);
    i++;
  }

  // 5. Final validation
  cr_assert_eq(i, 3, "Iterator did not loop the correct number of times");
  cr_assert(tk_iter_equal(&it, &end),
            "Iterator did not equal end() after the loop");
}

// --- Test helpers for tk_vec_destroy_full ---

/**
 * @brief A global counter to be incremented by the test destroyer.
 */
static int g_destroy_counter = 0;

/**
 * @brief A test destroyer function that increments a global counter.
 * @param element_ptr Pointer to the element (unused in this test).
 */
static void test_element_destroyer(void *element_ptr) {
  (void)element_ptr; // Suppress unused warning
  g_destroy_counter++;
}

// --- Standalone Miscellaneous Tests ---

typedef struct {
  long long id;
  char name[16];
} complex_data_t;

Test(misc_tests, struct_vector) {
  tk_vec_t *struct_vec = tk_vec_create(sizeof(complex_data_t));
  cr_assert_not_null(struct_vec);

  for (int i = 0; i < 100; ++i) {
    complex_data_t data = {.id = i * 1000};
    snprintf(data.name, 16, "Entry %d", i);
    tk_vec_push_back(struct_vec, &data);
  }

  complex_data_t *d1 = (complex_data_t *)tk_vec_at(struct_vec, 10);
  cr_assert_not_null(d1);
  cr_assert_eq(d1->id, 10000);
  cr_assert_str_eq(d1->name, "Entry 10");

  tk_vec_destroy(struct_vec);
}

Test(misc_tests, float_vector) {
  tk_vec_t *float_vec = tk_vec_create(sizeof(float));
  cr_assert_not_null(float_vec);

  float f1 = 1.1f, f2 = 2.2f;
  tk_vec_push_back(float_vec, &f1);
  tk_vec_push_back(float_vec, &f2);

  const float epsilon = 0.00001f;
  cr_assert_float_eq(*(float *)tk_vec_front(float_vec), 1.1f, epsilon);
  cr_assert_float_eq(*(float *)tk_vec_back(float_vec), 2.2f, epsilon);

  tk_vec_destroy(float_vec);
}

Test(misc_tests, boundary_checks) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);

  // Test access on an empty vector
  cr_assert_null(tk_vec_front(v), "front() on empty vector should be NULL");
  cr_assert_null(tk_vec_back(v), "back() on empty vector should be NULL");
  cr_assert_null(tk_vec_at(v, 0), "at(0) on empty vector should be NULL");

  // Add one element
  int val = 42;
  tk_vec_push_back(v, &val);

  // Test out-of-bounds access
  cr_assert_null(tk_vec_at(v, 1), "at(size) should be out of bounds");
  cr_assert_null(tk_vec_at(v, 100), "at(large_index) should be out of bounds");

  tk_vec_destroy(v);
}

/**
 * @brief Tests the tk_vec_destroy_full function to ensure
 * the destroyer is called for each element.
 */
Test(misc_tests, destroy_full) {
  // 1. Reset the global counter
  g_destroy_counter = 0;

  // 2. Create a vector and add 5 elements
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 0; i < 5; ++i) {
    tk_vec_push_back(v, &i);
  }
  cr_assert_eq(tk_vec_size(v), 5);

  // 3. Call destroy_full() with our test destroyer
  // This function call will free 'v', so we must not use 'v' afterward.
  tk_vec_destroy_full(v, test_element_destroyer);

  // 4. Assert that the destroyer was called 5 times
  cr_assert_eq(g_destroy_counter, 5,
               "The destroyer function was not called the correct number of "
               "times.");
}
