/**
 * @file test_sequence.c
 * @brief Unit tests for the generic sequence algorithms in
 * <tk/algo/sequence.h>.
 *
 * This file tests the generic algorithms using `tk_vec_t` as the underlying
 * concrete container. This validates that the algorithms correctly use the
 * polymorphic `tk_iterator_t` interface.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <tk/algo/sequence.h> // The algorithms we are testing
#include <tk/core/iterator.h> // The iterator interface
#include <tk/ds/vec.h>        // The container we are testing with

// --- Test Fixture ---

static tk_vec_t *vec_int;

// Setup: Create a vector and populate it with {10, 20, 30, 40, 50}
void setup_algo_suite(void) {
  vec_int = tk_vec_create(sizeof(int)); //
  cr_assert_not_null(vec_int, "Vector creation failed");

  for (int i = 1; i <= 5; ++i) {
    int val = i * 10;
    tk_vec_push_back(vec_int, &val); //
  }
  // vec_int now contains: {10, 20, 30, 40, 50}
}

void teardown_algo_suite(void) {
  tk_vec_destroy(vec_int); //
}

TestSuite(algo_suite, .init = setup_algo_suite, .fini = teardown_algo_suite);

// --- Predicate functions for testing ---

/**
 * @brief Predicate function that returns true if the integer is 30.
 */
static tk_bool find_30(const void *element) {
  return (*(const int *)element) == 30;
}

/**
 * @brief Predicate function that returns true if the integer is odd.
 */
static tk_bool find_odd(const void *element) {
  return (*(const int *)element) % 2 != 0;
}

/**
 * @brief Predicate function that always returns false (to test not-found).
 */
static tk_bool find_99(const void *element) {
  return (*(const int *)element) == 99;
}

/**
 * @brief [NEW] Predicate function that returns true if the integer is less
 * than 15.
 */
static tk_bool find_less_than_15(const void *element) {
  return (*(const int *)element) < 15;
}

// --- Test Cases ---

/**
 * @brief Test `tk_algo_find_if` when the element exists.
 */
Test(algo_suite, find_if_exists) {
  // Get the begin and end iterators for our vector
  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  // Call the generic algorithm
  tk_iterator_t result = tk_algo_find_if(begin, end, find_30);

  // Check that the result is not the 'end' iterator
  cr_assert(
      tk_iter_equal(&result, &end) == false,
      "tk_algo_find_if should not return 'end' when an element is found.");

  // Check that the found element is correct
  void *data = tk_iter_get(&result);
  cr_assert_not_null(data, "tk_iter_get() returned NULL on a valid iterator");
  cr_assert_eq(*(int *)data, 30, "The algorithm found the wrong element.");
}

/**
 * @brief Test `tk_algo_find_if` when the element does not exist.
 */
Test(algo_suite, find_if_not_exists) {
  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  // Call the generic algorithm with a predicate that always fails
  tk_iterator_t result = tk_algo_find_if(begin, end, find_99);

  // Check that the result IS the 'end' iterator
  cr_assert(tk_iter_equal(&result, &end) == true,
            "tk_algo_find_if should return 'end' when no element is found.");
}

/**
 * @brief Test `tk_algo_find_if` on an empty vector.
 */
Test(algo_suite, find_if_empty_vector) {
  // Clear the vector to make it empty
  tk_vec_clear(vec_int);

  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  // Check that begin and end are correctly equal
  cr_assert(tk_iter_equal(&begin, &end) == true,
            "begin() and end() should be equal on an empty vector.");

  // Call the algorithm
  tk_iterator_t result = tk_algo_find_if(begin, end, find_30);

  // Check that the result is still 'end'
  cr_assert(tk_iter_equal(&result, &end) == true,
            "tk_algo_find_if should return 'end' when run on an empty range.");
}

/**
 * @brief Test `tk_algo_find_if` for the first element.
 */
Test(algo_suite, find_if_first_element) {
  // Our vector is {10, 20, 30, 40, 50}
  // Let's find the first element (10) by finding the first element < 15
  tk_iterator_t begin = tk_vec_begin(vec_int);
  tk_iterator_t end = tk_vec_end(vec_int);

  // Replaced the C++ lambda with a C function pointer
  tk_iterator_t result = tk_algo_find_if(begin, end, find_less_than_15);

  // Check that the result is not 'end'
  cr_assert(tk_iter_equal(&result, &end) == false,
            "Algorithm failed to find the first element.");

  // Check that the result is equal to 'begin'
  cr_assert(
      tk_iter_equal(&result, &begin) == true,
      "Algorithm should have returned the 'begin' iterator, but it didn't.");

  // Check the value
  cr_assert_eq(*(int *)tk_iter_get(&result), 10,
               "The algorithm found the wrong element.");
}
