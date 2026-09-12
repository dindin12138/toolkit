/**
 * @file test_headers.c
 * @brief Regression tests for header composition (issue #1 / #6).
 *
 * Under strict C99 a type may only be defined once per translation unit. The
 * `tk_element_destroyer_t` typedef used to be declared in *both* <tk/ds/vec.h>
 * and <tk/ds/list.h>, so any translation unit that included both container
 * headers failed to compile with `-Wtypedef-redefinition`. This test includes
 * both headers in one TU; if the duplicate typedef ever returns, this file will
 * not compile.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

/**
 * @brief A destroyer that is compatible with both containers, proving the
 * destroyer type has a single, shared definition.
 */
static void counting_destroyer(void *element_ptr) {
  (void)element_ptr; // Suppress unused warning
}

Test(header_inclusion, both_container_headers_in_one_tu) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(v, "vector creation failed");
  cr_assert_not_null(l, "list creation failed");

  int a = 1, b = 2;
  cr_assert_eq(tk_vec_push_back(v, &a), TK_SUCCESS);
  cr_assert_eq(tk_list_push_back(l, &b), TK_SUCCESS);
  cr_assert_eq(tk_vec_size(v), 1);
  cr_assert_eq(tk_list_size(l), 1);

  tk_vec_destroy(v);
  tk_list_destroy(l);
}

Test(header_inclusion, shared_destroyer_type) {
  // The same destroyer type must be usable with both containers.
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  int x = 1;
  tk_vec_push_back(v, &x);
  tk_vec_destroy_full(v, counting_destroyer); // frees v

  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  tk_list_push_back(l, &x);
  tk_list_destroy_full(l, counting_destroyer); // frees l

  cr_assert(true, "Both containers accepted the shared destroyer type");
}
