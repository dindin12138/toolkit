/**
 * @file test_list.c
 * @brief Unit tests for the tk_list module using the Criterion framework.
 *
 * This file tests the functionality of the doubly-linked list implementation,
 * including its lifecycle, modification operations, and bidirectional
 * iterators.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h> // Modern assertion macros (eq, ne, etc.)
#include <stdio.h>
#include <tk/core/iterator.h> // For tk_iterator_t and operations
#include <tk/ds/list.h>       // The list implementation we are testing

// --- Test Fixture for Integer List ---

static tk_list_t *list_int;

// Setup: Create an empty list for integers before each test
void setup_list(void) {
  list_int = tk_list_create(sizeof(int));
  cr_assert_not_null(list_int, "List creation failed in setup");
}

// Teardown: Destroy the list after each test
void teardown_list(void) { tk_list_destroy(list_int); }

// Define the test suite with setup and teardown functions
TestSuite(list_suite, .init = setup_list, .fini = teardown_list);

// --- Test Cases for Integer List Suite ---

/**
 * @brief Test basic lifecycle functions: create, size, is_empty.
 */
Test(list_suite, lifecycle) {
  cr_assert_eq(tk_list_size(list_int), 0, "Initial size should be 0");     //
  cr_assert(tk_list_is_empty(list_int), "List should be empty initially"); //
  // Note: Lists don't typically expose capacity like vectors.
}

/**
 * @brief Test adding elements to the back and removing them.
 */
Test(list_suite, push_pop_back) {
  int val1 = 10, val2 = 20;

  tk_list_push_back(list_int, &val1); //
  cr_assert_eq(tk_list_size(list_int), 1);
  cr_assert_eq(*(int *)tk_list_back(list_int), 10);  //
  cr_assert_eq(*(int *)tk_list_front(list_int), 10); //

  tk_list_push_back(list_int, &val2);
  cr_assert_eq(tk_list_size(list_int), 2);
  cr_assert_eq(*(int *)tk_list_back(list_int), 20);
  cr_assert_eq(*(int *)tk_list_front(list_int), 10); // Front should remain 10

  tk_list_pop_back(list_int); //
  cr_assert_eq(tk_list_size(list_int), 1);
  cr_assert_eq(*(int *)tk_list_back(list_int), 10);
  cr_assert_eq(*(int *)tk_list_front(list_int), 10);

  tk_list_pop_back(list_int);
  cr_assert_eq(tk_list_size(list_int), 0);
  cr_assert(tk_list_is_empty(list_int));
  cr_assert_null(tk_list_back(list_int), "Back should be NULL on empty list");
  cr_assert_null(tk_list_front(list_int), "Front should be NULL on empty list");
}

/**
 * @brief Test adding elements to the front and removing them.
 */
Test(list_suite, push_pop_front) {
  int val1 = 10, val2 = 20;

  tk_list_push_front(list_int, &val1); //
  cr_assert_eq(tk_list_size(list_int), 1);
  cr_assert_eq(*(int *)tk_list_front(list_int), 10);
  cr_assert_eq(*(int *)tk_list_back(list_int), 10);

  tk_list_push_front(list_int, &val2); // list is now {20, 10}
  cr_assert_eq(tk_list_size(list_int), 2);
  cr_assert_eq(*(int *)tk_list_front(list_int), 20);
  cr_assert_eq(*(int *)tk_list_back(list_int), 10); // Back should remain 10

  tk_list_pop_front(list_int); // list is now {10}
  cr_assert_eq(tk_list_size(list_int), 1);
  cr_assert_eq(*(int *)tk_list_front(list_int), 10);
  cr_assert_eq(*(int *)tk_list_back(list_int), 10);

  tk_list_pop_front(list_int);
  cr_assert_eq(tk_list_size(list_int), 0);
  cr_assert(tk_list_is_empty(list_int));
  cr_assert_null(tk_list_front(list_int));
  cr_assert_null(tk_list_back(list_int));
}

/**
 * @brief Test clearing the list.
 */
Test(list_suite, clear) {
  int i;
  for (i = 0; i < 50; ++i) {
    tk_list_push_back(list_int, &i);
  }
  cr_assert_eq(tk_list_size(list_int), 50);

  tk_list_clear(list_int); //
  cr_assert_eq(tk_list_size(list_int), 0, "Size should be 0 after clear");
  cr_assert(tk_list_is_empty(list_int), "List should be empty after clear");
}

/**
 * @brief Test basic boundary conditions like accessing front/back of empty
 * list.
 */
Test(list_suite, boundary_checks) {
  // list_int is empty initially due to fixture setup
  cr_assert_null(tk_list_front(list_int),
                 "front() on empty list should be NULL"); //
  cr_assert_null(tk_list_back(list_int),
                 "back() on empty list should be NULL"); //
  cr_assert_null(tk_list_front_mut(list_int),
                 "front_mut() on empty list should be NULL"); //
  cr_assert_null(tk_list_back_mut(list_int),
                 "back_mut() on empty list should be NULL"); //

  // Test pop on empty list (should do nothing gracefully)
  tk_list_pop_back(list_int);  //
  tk_list_pop_front(list_int); //
  cr_assert(tk_list_is_empty(list_int));

  // Add one element
  int val = 42;
  tk_list_push_back(list_int, &val);
  cr_assert_not_null(tk_list_front(list_int));
  cr_assert_not_null(tk_list_back(list_int));

  // Pop it
  tk_list_pop_back(list_int);
  cr_assert(tk_list_is_empty(list_int));
}

/**
 * @brief Validates the bidirectional iterator protocol for tk_list_t.
 * Checks begin(), end(), next(), prev(), get(), equal(), clone().
 */
Test(list_suite, iterators_bidirectional) {
  // 1. Test empty list
  tk_iterator_t begin_empty = tk_list_begin(list_int); //
  tk_iterator_t end_empty = tk_list_end(list_int);     //
  cr_assert(tk_iter_equal(&begin_empty, &end_empty),
            "begin() and end() should be equal on an empty list"); //
  // Test retreating from end() on empty list should yield begin() (which is
  // also end())
  tk_iterator_t prev_from_end_empty = end_empty;
  tk_iter_prev(&prev_from_end_empty); //
  cr_assert(tk_iter_equal(&prev_from_end_empty, &begin_empty),
            "prev() from end() on empty list should result in begin()");

  // 2. Test populated list {10, 20, 30}
  int v1 = 10, v2 = 20, v3 = 30;
  tk_list_push_back(list_int, &v1);
  tk_list_push_back(list_int, &v2);
  tk_list_push_back(list_int, &v3);

  int expected_values[] = {10, 20, 30};
  int i = 0;

  tk_iterator_t it = tk_list_begin(list_int);
  tk_iterator_t end = tk_list_end(list_int);

  cr_assert(tk_iter_equal(&it, &end) == false,
            "begin() and end() should not be equal on a populated list");

  // 3. Test tk_iter_clone()
  tk_iterator_t clone_it;
  tk_iter_clone(&clone_it, &it);
  cr_assert(tk_iter_equal(&clone_it, &it), "Cloned iterator should be equal");
  tk_iter_next(&it); //
  cr_assert(tk_iter_equal(&clone_it, &it) == false,
            "Cloned iter independent after next()");
  cr_assert_eq(*(int *)tk_iter_get(&clone_it), 10,
               "Clone retained position"); //

  // 4. Test forward iteration loop (next, get, equal)
  it = tk_list_begin(list_int); // Reset
  i = 0;
  while (!tk_iter_equal(&it, &end)) {
    cr_assert_lt(i, 3, "Forward loop ran too many times");
    void *data = tk_iter_get(&it);
    cr_assert_not_null(data, "tk_iter_get() returned NULL");
    cr_assert_eq(*(int *)data, expected_values[i],
                 "Forward value mismatch at index %d", i);
    tk_iter_next(&it);
    i++;
  }
  cr_assert_eq(i, 3, "Forward loop count incorrect");
  cr_assert(tk_iter_equal(&it, &end),
            "Iterator not at end() after forward loop");

  // 5. Test backward iteration loop (prev, get, equal)
  // Start from one position *before* end (i.e., the last element)
  tk_iterator_t rit = end;
  tk_iter_prev(&rit); // Move rit to point to 30
  i = 2;              // Index of the last element
  tk_iterator_t begin = tk_list_begin(list_int);

  while (
      true) { // Loop until we manually break after processing the first element
    cr_assert_geq(i, 0, "Backward loop ran too many times");
    void *data = tk_iter_get(&rit);
    cr_assert_not_null(data,
                       "tk_iter_get() returned NULL during backward loop");
    cr_assert_eq(*(int *)data, expected_values[i],
                 "Backward value mismatch at index %d", i);

    // Check if we are at the beginning *before* retreating further
    if (tk_iter_equal(&rit, &begin)) {
      break; // Processed the first element, exit loop
    }

    tk_iter_prev(&rit);
    i--;
  }
  cr_assert_eq(i, 0, "Backward loop did not end on the first element");
  cr_assert(tk_iter_equal(&rit, &begin),
            "Iterator not at begin() after backward loop");
}

/**
 * @brief Tests iterator-based insertion (tk_list_insert_before).
 */
Test(list_suite, iterators_insert) {
  int v1 = 10, v2 = 20, v3 = 30, v_new = 99;

  // 1. Insert into empty list (before end())
  tk_iterator_t end_empty = tk_list_end(list_int);
  tk_list_insert_before(list_int, end_empty, &v1); //
  cr_assert_eq(tk_list_size(list_int), 1);
  cr_assert_eq(*(int *)tk_list_front(list_int), 10);

  // 2. Insert at the beginning (before begin())
  tk_iterator_t begin1 = tk_list_begin(list_int);
  tk_list_insert_before(list_int, begin1, &v_new); // {99, 10}
  cr_assert_eq(tk_list_size(list_int), 2);
  cr_assert_eq(*(int *)tk_list_front(list_int), 99);
  cr_assert_eq(*(int *)tk_list_back(list_int), 10);

  // 3. Insert at the end (before end())
  tk_iterator_t end2 = tk_list_end(list_int);
  tk_list_insert_before(list_int, end2, &v2); // {99, 10, 20}
  cr_assert_eq(tk_list_size(list_int), 3);
  cr_assert_eq(*(int *)tk_list_back(list_int), 20);

  // 4. Insert in the middle (before the element '10')
  tk_iterator_t it = tk_list_begin(list_int); // points to 99
  tk_iter_next(&it);                          // points to 10
  tk_list_insert_before(list_int, it, &v3);   // {99, 30, 10, 20}
  cr_assert_eq(tk_list_size(list_int), 4);
  // Verify order
  it = tk_list_begin(list_int);
  cr_assert_eq(*(int *)tk_iter_get(&it), 99);
  tk_iter_next(&it);
  cr_assert_eq(*(int *)tk_iter_get(&it), 30);
  tk_iter_next(&it);
  cr_assert_eq(*(int *)tk_iter_get(&it), 10);
  tk_iter_next(&it);
  cr_assert_eq(*(int *)tk_iter_get(&it), 20);
  tk_iter_next(&it);
  tk_iterator_t end_iter = tk_list_end(list_int);
  cr_assert(tk_iter_equal(&it, &end_iter));
}

/**
 * @brief Tests iterator-based erasure (tk_list_erase_at).
 */
Test(list_suite, iterators_erase) {
  int v1 = 10, v2 = 20, v3 = 30, v4 = 40;
  tk_list_push_back(list_int, &v1);
  tk_list_push_back(list_int, &v2);
  tk_list_push_back(list_int, &v3);
  tk_list_push_back(list_int, &v4); // List is now {10, 20, 30, 40}

  tk_iterator_t it, next_it;
  tk_iterator_t current_begin; // Variable to store begin iterator
  tk_iterator_t current_end;   // Variable to store end iterator

  // 1. Erase from the middle (erase 20)
  it = tk_list_begin(list_int);             // -> 10
  tk_iter_next(&it);                        // -> 20
  next_it = tk_list_erase_at(list_int, it); // list is {10, 30, 40}
  cr_assert_eq(tk_list_size(list_int), 3);
  cr_assert_not_null(next_it.vtable,
                     "erase middle should return valid iterator");
  cr_assert_eq(*(int *)tk_iter_get(&next_it), 30,
               "erase middle should return iterator to next element");
  // Verify list content
  it = tk_list_begin(list_int);
  cr_assert_eq(*(int *)tk_iter_get(&it), 10);
  tk_iter_next(&it);
  cr_assert_eq(*(int *)tk_iter_get(&it), 30);
  tk_iter_next(&it);
  cr_assert_eq(*(int *)tk_iter_get(&it), 40);
  tk_iter_next(&it);
  current_end = tk_list_end(list_int);         // Store end iterator
  cr_assert(tk_iter_equal(&it, &current_end)); // Use variable address

  // 2. Erase the head (erase 10)
  it = tk_list_begin(list_int);             // -> 10
  next_it = tk_list_erase_at(list_int, it); // list is {30, 40}
  cr_assert_eq(tk_list_size(list_int), 2);
  cr_assert_not_null(next_it.vtable, "erase head should return valid iterator");
  cr_assert_eq(*(int *)tk_iter_get(&next_it), 30,
               "erase head should return iterator to new head");
  current_begin = tk_list_begin(list_int);           // Store new begin iterator
  cr_assert(tk_iter_equal(&next_it, &current_begin), // Use variable address
            "Returned iterator should be the new begin()");
  cr_assert_eq(*(int *)tk_list_front(list_int), 30);

  // 3. Erase the tail (erase 40)
  it = tk_list_begin(list_int);             // -> 30
  tk_iter_next(&it);                        // -> 40
  next_it = tk_list_erase_at(list_int, it); // list is {30}
  cr_assert_eq(tk_list_size(list_int), 1);
  cr_assert_not_null(next_it.vtable, "erase tail should return valid iterator");
  current_end = tk_list_end(list_int);             // Store end iterator
  cr_assert(tk_iter_equal(&next_it, &current_end), // Use variable address
            "erase tail should return end() iterator");
  cr_assert_eq(*(int *)tk_list_back(list_int), 30);
  cr_assert_eq(*(int *)tk_list_front(list_int), 30);

  // 4. Erase the last remaining element (erase 30)
  it = tk_list_begin(list_int);             // -> 30
  next_it = tk_list_erase_at(list_int, it); // list is {}
  cr_assert_eq(tk_list_size(list_int), 0);
  cr_assert(tk_list_is_empty(list_int));
  cr_assert_not_null(next_it.vtable, "erase last should return valid iterator");
  current_end = tk_list_end(list_int);             // Store end iterator
  cr_assert(tk_iter_equal(&next_it, &current_end), // Use variable address
            "erase last should return end() iterator");
  current_begin =
      tk_list_begin(list_int); // Store begin iterator (which is also end)
  current_end = tk_list_end(list_int); // Store end iterator again
  cr_assert(
      tk_iter_equal(&current_begin, &current_end), // Use variable addresses
      "begin() == end() after erasing last");

  // 5. Attempt to erase end() iterator (should be safe no-op returning invalid
  // iter)
  it = tk_list_end(list_int);
  next_it = tk_list_erase_at(list_int, it);
  cr_assert_null(next_it.vtable,
                 "Erasing end() should return invalid iterator");
  cr_assert(tk_list_is_empty(list_int)); // List should remain empty

  // 6. Attempt to erase from empty list (should be safe no-op returning invalid
  // iter)
  it = tk_list_begin(list_int);             // begin() == end()
  next_it = tk_list_erase_at(list_int, it); // Trying to erase end() essentially
  cr_assert_null(next_it.vtable,
                 "Erasing from empty list should return invalid iterator");
}

// --- Test helpers for tk_list_destroy_full ---

static int g_list_destroy_counter = 0;

static void test_list_element_destroyer(void *element_data) {
  // We receive the pointer to the data itself (e.g., int*)
  // Increment counter. In a real scenario, might free(*((char**)element_ptr))
  (void)element_data; // Suppress unused warning
  g_list_destroy_counter++;
}

// --- Standalone Test for destroy_full ---
// Needs to be standalone as the fixture uses tk_list_destroy

/**
 * @brief Tests tk_list_destroy_full ensures the destroyer is called.
 */
Test(standalone_list_tests, destroy_full) {
  // 1. Reset the global counter
  g_list_destroy_counter = 0;

  // 2. Create a list and add 5 elements
  tk_list_t *lst = tk_list_create(sizeof(int));
  cr_assert_not_null(lst);
  for (int i = 0; i < 5; ++i) {
    tk_list_push_back(lst, &i);
  }
  cr_assert_eq(tk_list_size(lst), 5);

  // 3. Call destroy_full() with our test destroyer
  // This function call will free 'lst'
  tk_list_destroy_full(lst, test_list_element_destroyer); //

  // 4. Assert that the destroyer was called 5 times
  cr_assert_eq(g_list_destroy_counter, 5,
               "The destroyer function was not called the correct number of "
               "times.");
}
