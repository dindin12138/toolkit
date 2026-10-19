/**
 * @file test_iterator_element_size.c
 * @brief Tests for element size as a mandatory part of the iterator protocol
 * (issue #2).
 *
 * Covers:
 *  - both containers report the byte size of one element, from begin() AND
 *    from end(), so an empty range still answers;
 *  - an invalid iterator reports 0 instead of dereferencing anything;
 *  - the vtable validator rejects a vtable that omits the accessor, for every
 *    iterator category;
 *  - the three vtable-definition macros wire the container's own accessor, so
 *    a new container cannot register a vtable without implementing one.
 */

#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include <signal.h>   // SIGABRT
#include <stdint.h>   // int64_t (fixed-width element types)
#include <sys/wait.h> // waitpid, WIFSIGNALED, WTERMSIG
#include <unistd.h>   // fork, _exit, pid_t

#include <tk/core/iterator.h>
#include <tk/ds/list.h>
#include <tk/ds/vec.h>

// A 16-byte element: element size must never be confused with the size of a
// pointer or with the size of the first field.
typedef struct {
  int64_t a;
  int64_t b;
} pair16_t;

// --- Both containers report the size of one element ---

Test(iterator_element_size, vec_reports_element_size) {
  tk_vec_t *v = tk_vec_create(sizeof(pair16_t));
  cr_assert_not_null(v);
  pair16_t p = {1, 2};
  tk_vec_push_back(v, &p);

  tk_iterator_t begin = tk_vec_begin(v);
  tk_iterator_t end = tk_vec_end(v);

  cr_assert_eq(tk_iter_element_size(&begin), (size_t)16,
               "vec begin() must report the byte size of one element");
  cr_assert_eq(tk_iter_element_size(&end), (size_t)16,
               "vec end() must report the byte size of one element");

  tk_vec_destroy(v);
}

Test(iterator_element_size, list_reports_element_size) {
  tk_list_t *l = tk_list_create(sizeof(pair16_t));
  cr_assert_not_null(l);
  pair16_t p = {1, 2};
  tk_list_push_back(l, &p);

  tk_iterator_t begin = tk_list_begin(l);
  tk_iterator_t end = tk_list_end(l);

  cr_assert_eq(tk_iter_element_size(&begin), (size_t)16,
               "list begin() must report the byte size of one element");
  cr_assert_eq(tk_iter_element_size(&end), (size_t)16,
               "list end() must report the byte size of one element");

  tk_list_destroy(l);
}

Test(iterator_element_size, empty_range_still_reports_element_size) {
  // An empty range is exactly the case where begin() == end(): the answer must
  // not depend on there being a dereferenceable element.
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  tk_iterator_t vbegin = tk_vec_begin(v);
  tk_iterator_t vend = tk_vec_end(v);
  cr_assert_eq(tk_iter_element_size(&vbegin), sizeof(int),
               "empty vec begin() must still report the element size");
  cr_assert_eq(tk_iter_element_size(&vend), sizeof(int),
               "empty vec end() must still report the element size");

  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  tk_iterator_t lbegin = tk_list_begin(l);
  tk_iterator_t lend = tk_list_end(l);
  cr_assert_eq(tk_iter_element_size(&lbegin), sizeof(int),
               "empty list begin() must still report the element size");
  cr_assert_eq(tk_iter_element_size(&lend), sizeof(int),
               "empty list end() must still report the element size");

  tk_vec_destroy(v);
  tk_list_destroy(l);
}

// --- An invalid iterator answers 0 instead of dereferencing anything ---

Test(iterator_element_size, invalid_iterator_reports_zero) {
  tk_iterator_t inv = {.vtable = NULL};
  cr_assert_eq(tk_iter_element_size(&inv), (size_t)0,
               "an invalid iterator must report an element size of 0");
}

Test(iterator_element_size, erase_at_invalid_iterator_reports_zero) {
  // tk_list_erase_at yields an invalid iterator on error; asking it for an
  // element size must be safe and answer 0.
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  tk_iterator_t inv = tk_list_erase_at(l, tk_list_end(l));
  cr_assert_null(inv.vtable);
  cr_assert_eq(tk_iter_element_size(&inv), (size_t)0,
               "an invalid iterator from erase_at must report 0");
  tk_list_destroy(l);
}

// ===========================================================================
// The three vtable-definition macros must reference the container's own
// accessor. Each fake iterator below implements `PREFIX##_element_size`
// returning a distinctive value, so the assertions prove the macro wired that
// exact symbol rather than defaulting to something (e.g. 0 or sizeof(void*)).
//
// The macros spell every slot as `PREFIX##_<slot>`, so each prefix needs its
// own symbol set. The generator marks every symbol unused: a FORWARD prefix
// never references `_retreat`, `_advance_by` or `_distance`.
// ===========================================================================

#define DEFINE_FAKE_ITERATOR(PREFIX, SIZE)                                     \
  static __attribute__((unused)) void PREFIX##_advance(tk_iterator_t *self) {  \
    (void)self;                                                                \
  }                                                                            \
  static __attribute__((unused)) void *PREFIX##_get(                           \
      const tk_iterator_t *self) {                                             \
    (void)self;                                                                \
    return NULL;                                                               \
  }                                                                            \
  static __attribute__((unused)) tk_bool PREFIX##_equal(                       \
      const tk_iterator_t *a, const tk_iterator_t *b) {                        \
    (void)a;                                                                   \
    (void)b;                                                                   \
    return false;                                                              \
  }                                                                            \
  static __attribute__((unused)) void PREFIX##_clone(tk_iterator_t *dest,      \
                                                     const tk_iterator_t *src) {\
    (void)dest;                                                                \
    (void)src;                                                                 \
  }                                                                            \
  static __attribute__((unused)) void PREFIX##_retreat(tk_iterator_t *self) {  \
    (void)self;                                                                \
  }                                                                            \
  static __attribute__((unused)) void PREFIX##_advance_by(                     \
      tk_iterator_t *self, ptrdiff_t n) {                                      \
    (void)self;                                                                \
    (void)n;                                                                   \
  }                                                                            \
  static __attribute__((unused)) ptrdiff_t PREFIX##_distance(                  \
      const tk_iterator_t *a, const tk_iterator_t *b) {                        \
    (void)a;                                                                   \
    (void)b;                                                                   \
    return 0;                                                                  \
  }                                                                            \
  static __attribute__((unused)) size_t PREFIX##_element_size(                 \
      const tk_iterator_t *self) {                                             \
    (void)self;                                                                \
    return (SIZE);                                                             \
  }

DEFINE_FAKE_ITERATOR(fwd_only, 7)   // forward-only: 7 bytes per element
DEFINE_FAKE_ITERATOR(bidir, 11)     // bidirectional: 11 bytes per element
DEFINE_FAKE_ITERATOR(rand_access, 13) // random access: 13 bytes per element

static const tk_iterator_vtable_t g_fwd_only_vtable =
    TK_DEFINE_FORWARD_ITERATOR_VTABLE(fwd_only, "fwd_only_iterator");

static const tk_iterator_vtable_t g_bidir_vtable =
    TK_DEFINE_ITERATOR_VTABLE(bidir, TK_ITER_BIDIRECTIONAL, "bidir_iterator");

static const tk_iterator_vtable_t g_rand_access_vtable =
    TK_DEFINE_RANDOM_ACCESS_ITERATOR_VTABLE(rand_access,
                                            "rand_access_iterator");

Test(iterator_element_size, macros_wire_the_containers_own_accessor) {
  tk_iterator_t it = {.vtable = &g_fwd_only_vtable};

  cr_assert_eq(g_fwd_only_vtable.element_size(&it), (size_t)7,
               "FORWARD macro must wire PREFIX##_element_size");
  cr_assert_eq(g_bidir_vtable.element_size(&it), (size_t)11,
               "BIDIRECTIONAL macro must wire PREFIX##_element_size");
  cr_assert_eq(g_rand_access_vtable.element_size(&it), (size_t)13,
               "RANDOM_ACCESS macro must wire PREFIX##_element_size");
}

Test(iterator_element_size, real_containers_wire_their_accessor) {
  tk_vec_t *v = tk_vec_create(sizeof(int64_t));
  cr_assert_not_null(v);
  tk_list_t *l = tk_list_create(sizeof(int64_t));
  cr_assert_not_null(l);

  tk_iterator_t vit = tk_vec_begin(v);
  tk_iterator_t lit = tk_list_begin(l);
  cr_assert_eq(vit.vtable->element_size(&vit), sizeof(int64_t),
               "the vec vtable's accessor must answer the container's size");
  cr_assert_eq(lit.vtable->element_size(&lit), sizeof(int64_t),
               "the list vtable's accessor must answer the container's size");

  tk_vec_destroy(v);
  tk_list_destroy(l);
}

// ===========================================================================
// Negative tests for tk_iterator_vtable_validate: a vtable that omits the
// element_size accessor must be rejected for EVERY iterator category.
//
// TK_ASSERT calls abort(), so as in test_iterator_random_access.c the validator
// runs in a forked grandchild and the test inspects the grandchild's exit
// status.
// ===========================================================================
#ifndef NDEBUG // TK_ASSERT is a no-op under NDEBUG.

// Legal vtables, one per category (positive controls).
static const tk_iterator_vtable_t g_good_forward = {
    .category = TK_ITER_FORWARD,
    .type_name = "good_forward",
    .element_size = fwd_only_element_size,
    .advance = fwd_only_advance,
    .get = fwd_only_get,
    .equal = fwd_only_equal,
    .clone = fwd_only_clone,
    .retreat = NULL,
    .advance_by = NULL,
    .distance = NULL};

static const tk_iterator_vtable_t g_good_bidir = {
    .category = TK_ITER_BIDIRECTIONAL,
    .type_name = "good_bidir",
    .element_size = bidir_element_size,
    .advance = bidir_advance,
    .get = bidir_get,
    .equal = bidir_equal,
    .clone = bidir_clone,
    .retreat = bidir_retreat,
    .advance_by = NULL,
    .distance = NULL};

static const tk_iterator_vtable_t g_good_random_access = {
    .category = TK_ITER_RANDOM_ACCESS,
    .type_name = "good_random_access",
    .element_size = rand_access_element_size,
    .advance = rand_access_advance,
    .get = rand_access_get,
    .equal = rand_access_equal,
    .clone = rand_access_clone,
    .retreat = rand_access_retreat,
    .advance_by = rand_access_advance_by,
    .distance = rand_access_distance};

// Illegal: each category with the element_size accessor omitted.
static const tk_iterator_vtable_t g_bad_forward_no_size = {
    .category = TK_ITER_FORWARD,
    .type_name = "bad_forward_no_size",
    .element_size = NULL,
    .advance = fwd_only_advance,
    .get = fwd_only_get,
    .equal = fwd_only_equal,
    .clone = fwd_only_clone,
    .retreat = NULL,
    .advance_by = NULL,
    .distance = NULL};

static const tk_iterator_vtable_t g_bad_bidir_no_size = {
    .category = TK_ITER_BIDIRECTIONAL,
    .type_name = "bad_bidir_no_size",
    .element_size = NULL,
    .advance = bidir_advance,
    .get = bidir_get,
    .equal = bidir_equal,
    .clone = bidir_clone,
    .retreat = bidir_retreat,
    .advance_by = NULL,
    .distance = NULL};

static const tk_iterator_vtable_t g_bad_random_access_no_size = {
    .category = TK_ITER_RANDOM_ACCESS,
    .type_name = "bad_random_access_no_size",
    .element_size = NULL,
    .advance = rand_access_advance,
    .get = rand_access_get,
    .equal = rand_access_equal,
    .clone = rand_access_clone,
    .retreat = rand_access_retreat,
    .advance_by = rand_access_advance_by,
    .distance = rand_access_distance};

// Runs the validator in a forked grandchild; true iff it was killed by SIGABRT.
static tk_bool validate_aborts(const tk_iterator_vtable_t *vt) {
  pid_t pid = fork();
  cr_assert(pid >= 0, "fork() failed");
  if (pid == 0) {
    tk_iterator_vtable_validate(vt); // must abort for an illegal vtable
    _exit(0);
  }
  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  cr_assert_eq(waited, pid, "waitpid() failed");
  return WIFSIGNALED(status) && (WTERMSIG(status) == SIGABRT);
}

// Runs the validator in a forked grandchild; true iff it exited 0.
static tk_bool validate_accepts(const tk_iterator_vtable_t *vt) {
  pid_t pid = fork();
  cr_assert(pid >= 0, "fork() failed");
  if (pid == 0) {
    tk_iterator_vtable_validate(vt);
    _exit(0);
  }
  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  cr_assert_eq(waited, pid, "waitpid() failed");
  return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

Test(iterator_element_size, validate_rejects_forward_without_element_size) {
  cr_assert(validate_aborts(&g_bad_forward_no_size),
            "validator must abort for a FORWARD vtable with no element_size");
}

Test(iterator_element_size, validate_rejects_bidir_without_element_size) {
  cr_assert(
      validate_aborts(&g_bad_bidir_no_size),
      "validator must abort for a BIDIRECTIONAL vtable with no element_size");
}

Test(iterator_element_size, validate_rejects_random_access_without_size) {
  cr_assert(validate_aborts(&g_bad_random_access_no_size),
            "validator must abort for a RANDOM_ACCESS vtable with no "
            "element_size");
}

Test(iterator_element_size, validate_accepts_well_formed_vtables) {
  // Positive controls: prove the negative tests are not "any call aborts".
  cr_assert(validate_accepts(&g_good_forward));
  cr_assert(validate_accepts(&g_good_bidir));
  cr_assert(validate_accepts(&g_good_random_access));
}

#endif // !NDEBUG
