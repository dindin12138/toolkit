/**
 * @file criterion_tests.c
 * @brief Unit tests for the tk_vec module using the Criterion framework.
 *
 * This file demonstrates modern C unit testing practices with Criterion,
 * including test suites, fixtures for setup/teardown, and rich assertions.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h> // Modern assertion macros (eq, ne, etc.)
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

Test(vec_suite, reallocation) {
  int num_elements = 1000;
  for (int i = 0; i < num_elements; ++i) {
    tk_vec_push_back(vec, &i);
  }
  cr_assert_eq(tk_vec_size(vec), num_elements,
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
