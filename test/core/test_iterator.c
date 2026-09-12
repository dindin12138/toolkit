/**
 * @file test_iterator.c
 * @brief Regression tests for the core iterator protocol
 * (<tk/core/iterator.h>).
 *
 * Covers:
 *  - #23: the tk_iter_* helpers are safe on an invalid iterator (vtable ==
 *         NULL) and never dereference a NULL vtable.
 *  - #24: TK_DEFINE_FORWARD_ITERATOR_VTABLE compiles for a forward-only
 *         iterator (which defines no `_retreat` symbol) and sets
 *         retreat == NULL.
 *  - #25: retreating before begin() is a well-defined no-op (clamped), for both
 *         the list (stays at begin, never jumps to end) and the vector.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

// --- A minimal forward-only iterator used to exercise
//     TK_DEFINE_FORWARD_ITERATOR_VTABLE (issue #24). It deliberately defines
//     no `_retreat` function. ---

typedef struct {
  const int *ptr; // current position
  const int *end; // one-past-the-last
} fake_fwd_state_t;

static void fake_fwd_advance(tk_iterator_t *self) {
  fake_fwd_state_t *s = (fake_fwd_state_t *)self->state.data;
  if (s->ptr < s->end)
    s->ptr++;
}

static void *fake_fwd_get(const tk_iterator_t *self) {
  const fake_fwd_state_t *s = (const fake_fwd_state_t *)self->state.data;
  return (void *)s->ptr;
}

static tk_bool fake_fwd_equal(const tk_iterator_t *iter1,
                              const tk_iterator_t *iter2) {
  const fake_fwd_state_t *s1 = (const fake_fwd_state_t *)iter1->state.data;
  const fake_fwd_state_t *s2 = (const fake_fwd_state_t *)iter2->state.data;
  return s1->ptr == s2->ptr;
}

static void fake_fwd_clone(tk_iterator_t *dest, const tk_iterator_t *src) {
  *dest = *src;
}

static const tk_iterator_vtable_t g_fake_fwd_vtable =
    TK_DEFINE_FORWARD_ITERATOR_VTABLE(fake_fwd, "fake_forward_iterator");

// --- #24: the FORWARD vtable macro must compile and set retreat = NULL ---

Test(iterator_protocol, forward_vtable_macro) {
  cr_assert_eq(g_fake_fwd_vtable.category, TK_ITER_FORWARD,
               "FORWARD vtable category must be TK_ITER_FORWARD");
  cr_assert_null(g_fake_fwd_vtable.retreat,
                 "FORWARD vtable must set retreat = NULL");
  cr_assert_not_null(g_fake_fwd_vtable.advance);
  cr_assert_not_null(g_fake_fwd_vtable.get);
  cr_assert_not_null(g_fake_fwd_vtable.equal);
  cr_assert_not_null(g_fake_fwd_vtable.clone);
  cr_assert_str_eq(g_fake_fwd_vtable.type_name, "fake_forward_iterator");

  // A full forward traversal using only the generic iterator operations.
  int data[3] = {1, 2, 3};
  tk_iterator_t it = {.vtable = &g_fake_fwd_vtable};
  tk_iterator_t end = {.vtable = &g_fake_fwd_vtable};
  fake_fwd_state_t *it_state = (fake_fwd_state_t *)it.state.data;
  fake_fwd_state_t *end_state = (fake_fwd_state_t *)end.state.data;
  it_state->ptr = &data[0];
  it_state->end = &data[3];
  end_state->ptr = &data[3];
  end_state->end = &data[3];

  int count = 0;
  while (!tk_iter_equal(&it, &end)) {
    cr_assert_lt(count, 3, "Forward traversal ran too many times");
    cr_assert_eq(*(int *)tk_iter_get(&it), data[count],
                 "Forward traversal value mismatch at index %d", count);
    tk_iter_next(&it);
    count++;
  }
  cr_assert_eq(count, 3, "Forward traversal should visit exactly 3 elements");
}

// --- #23: invalid iterator (vtable == NULL) safety ---

Test(iterator_protocol, invalid_iterator_is_safe) {
  tk_iterator_t inv = {.vtable = NULL};
  tk_iterator_t inv2 = {.vtable = NULL};

  // equal: an invalid iterator is never equal to anything. Previously two NULL
  // vtables compared equal and the code called NULL->equal, crashing.
  cr_assert(tk_iter_equal(&inv, &inv) == false,
            "an invalid iterator must not equal itself");
  cr_assert(tk_iter_equal(&inv, &inv2) == false,
            "two invalid iterators must not be equal");

  // get: returns NULL.
  cr_assert_null(tk_iter_get(&inv),
                 "get() on an invalid iterator must be NULL");

  // next / prev: no-ops (must not crash), iterator stays invalid.
  tk_iter_next(&inv);
  tk_iter_prev(&inv);
  cr_assert_null(inv.vtable, "next/prev must not revive an invalid iterator");

  // clone: propagates the invalid marker.
  tk_iterator_t dst = {.vtable = &g_fake_fwd_vtable};
  tk_iter_clone(&dst, &inv);
  cr_assert_null(dst.vtable, "clone() of an invalid iterator must be invalid");
}

Test(iterator_protocol, erase_at_invalid_iterator_is_safe) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);

  // Erasing on an empty list yields an invalid iterator.
  tk_iterator_t inv = tk_list_erase_at(l, tk_list_end(l));
  cr_assert_null(inv.vtable,
                 "erase_at on an empty list must yield an invalid iterator");

  // All generic operations must be safe on it.
  cr_assert(tk_iter_equal(&inv, &inv) == false);
  cr_assert_null(tk_iter_get(&inv));
  tk_iter_next(&inv);
  tk_iter_prev(&inv);
  tk_iterator_t clone;
  tk_iter_clone(&clone, &inv);
  cr_assert_null(clone.vtable);

  tk_list_destroy(l);
}

// --- #25: retreat before begin() is a clamped no-op ---

Test(iterator_protocol, list_retreat_clamps_at_begin) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  int a = 1, b = 2;
  tk_list_push_back(l, &a);
  tk_list_push_back(l, &b);

  tk_iterator_t begin = tk_list_begin(l);
  tk_iterator_t it = tk_list_begin(l);
  // Retreating at begin() must stay at begin() (never silently jump to end()).
  tk_iter_prev(&it);
  cr_assert(tk_iter_equal(&it, &begin),
            "retreat at begin() must remain at begin()");

  tk_list_destroy(l);
}

Test(iterator_protocol, vec_retreat_clamps_at_begin) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  int a = 1;
  tk_vec_push_back(v, &a);

  tk_iterator_t begin = tk_vec_begin(v);
  tk_iterator_t it = tk_vec_begin(v);
  // Retreating at begin() is a no-op and must not step out of bounds.
  tk_iter_prev(&it);
  cr_assert(tk_iter_equal(&it, &begin),
            "retreat at begin() must remain at begin()");

  tk_vec_destroy(v);
}
