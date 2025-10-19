/**
 * @file test_list_sequence.c
 * @brief Unit tests for generic sequence algorithms using tk_list_t.
 *
 * This file verifies that the generic algorithms in <tk/algo/sequence.h>
 * work correctly with the tk_list_t container, demonstrating iterator
 * polymorphism. It mirrors the tests in test_sequence.c but uses a list
 * instead of a vector.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <tk/algo/sequence.h> // The generic algorithms
#include <tk/core/iterator.h> // The iterator interface
#include <tk/ds/list.h>       // The list container we are testing with

// --- Test Fixture ---

static tk_list_t
    *list_int_algo; // Use a different name to avoid potential conflicts

// Setup: Create a list and populate it with {10, 20, 30, 40, 50}
// Same data as the vector test for direct comparison.
void setup_list_algo_suite(void) {
  list_int_algo = tk_list_create(sizeof(int)); // Use list create
  cr_assert_not_null(list_int_algo, "List creation failed");

  for (int i = 1; i <= 5; ++i) {
    int val = i * 10;
    tk_list_push_back(list_int_algo, &val); // Use list push_back
  }
  // list_int_algo now contains: {10, 20, 30, 40, 50}
}

// Teardown: Destroy the list
void teardown_list_algo_suite(void) {
  tk_list_destroy(list_int_algo); // Use list destroy
}

// Define the test suite using the list fixture
TestSuite(list_algo_suite, .init = setup_list_algo_suite,
          .fini = teardown_list_algo_suite);

// --- Predicate functions (copied from test_sequence.c for simplicity) ---
// These predicates are generic and work on const void*

static tk_bool find_30(const void *element) {
  return (*(const int *)element) == 30;
}

static tk_bool find_odd(const void *element) {
  return (*(const int *)element) % 2 != 0;
}

static tk_bool find_99(const void *element) {
  return (*(const int *)element) == 99;
}

static tk_bool find_less_than_15(const void *element) {
  return (*(const int *)element) < 15;
}

// --- Test Cases (Identical logic to test_sequence.c, just using list
// iterators) ---

/**
 * @brief Test `tk_algo_find_if` on a list when the element exists.
 *
 */
Test(list_algo_suite, find_if_exists) {
  // Get list iterators
  tk_iterator_t begin = tk_list_begin(list_int_algo); //
  tk_iterator_t end = tk_list_end(list_int_algo);     //

  // Call the SAME generic algorithm
  tk_iterator_t result = tk_algo_find_if(begin, end, find_30); //

  // Assertions remain the same
  tk_iterator_t current_end = tk_list_end(list_int_algo);  // Store end iterator
  cr_assert(tk_iter_equal(&result, &current_end) == false, //
            "tk_algo_find_if should not return 'end' when element is found.");

  void *data = tk_iter_get(&result); //
  cr_assert_not_null(data, "tk_iter_get() returned NULL on a valid iterator");
  cr_assert_eq(*(int *)data, 30, "The algorithm found the wrong element.");
}

/**
 * @brief Test `tk_algo_find_if` on a list when the element does not exist.
 *
 */
Test(list_algo_suite, find_if_not_exists) {
  tk_iterator_t begin = tk_list_begin(list_int_algo);
  tk_iterator_t end = tk_list_end(list_int_algo);

  // Call the SAME generic algorithm
  tk_iterator_t result = tk_algo_find_if(begin, end, find_99);

  // Assertions remain the same
  tk_iterator_t current_end = tk_list_end(list_int_algo); // Store end iterator
  cr_assert(tk_iter_equal(&result, &current_end) == true,
            "tk_algo_find_if should return 'end' when no element is found.");
}

/**
 * @brief Test `tk_algo_find_if` on an empty list.
 *
 */
Test(list_algo_suite, find_if_empty_list) {
  // Clear the list to make it empty
  tk_list_clear(list_int_algo); // Use list clear

  tk_iterator_t begin = tk_list_begin(list_int_algo);
  tk_iterator_t end = tk_list_end(list_int_algo);

  // Check that begin and end are correctly equal
  cr_assert(tk_iter_equal(&begin, &end) == true,
            "begin() and end() should be equal on an empty list.");

  // Call the SAME algorithm
  tk_iterator_t result = tk_algo_find_if(begin, end, find_30);

  // Check that the result is still 'end'
  tk_iterator_t current_end = tk_list_end(list_int_algo); // Store end iterator
  cr_assert(tk_iter_equal(&result, &current_end) == true,
            "tk_algo_find_if should return 'end' when run on an empty range.");
}

/**
 * @brief Test `tk_algo_find_if` for the first element in a list.
 *
 */
Test(list_algo_suite, find_if_first_element) {
  // Our list is {10, 20, 30, 40, 50}
  tk_iterator_t begin = tk_list_begin(list_int_algo);
  tk_iterator_t end = tk_list_end(list_int_algo);

  // Call the SAME algorithm
  tk_iterator_t result = tk_algo_find_if(begin, end, find_less_than_15);

  // Assertions remain the same
  tk_iterator_t current_end = tk_list_end(list_int_algo); // Store end iterator
  cr_assert(tk_iter_equal(&result, &current_end) == false,
            "Algorithm failed to find the first element.");

  // Check that the result is equal to 'begin'
  // Note: We need to store begin again or use a copy, as 'begin' was passed by
  // value
  tk_iterator_t current_begin = tk_list_begin(list_int_algo);
  cr_assert(tk_iter_equal(&result, &current_begin) == true,
            "Algorithm should have returned the 'begin' iterator.");

  // Check the value
  cr_assert_eq(*(int *)tk_iter_get(&result), 10,
               "The algorithm found the wrong element.");
}
