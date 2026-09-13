/**
 * @file test_iterator_random_access.c
 * @brief Tests for the random-access vtable slots added in stage 1 (P1-1):
 *        `advance_by` / `distance`, the
 *        TK_DEFINE_RANDOM_ACCESS_ITERATOR_VTABLE macro, the symmetric vtable
 *        validation rules, and the tk_iter_advance_by / tk_iter_distance
 *        wrappers.
 *
 * The tests use tk_vec_t (random access) and tk_list_t (bidirectional) to
 * prove that the random-access capability is expressed through the protocol
 * and that it is NOT advertised by a non-random-access container.
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

// Compile-time proof that the two new vtable slots did NOT grow the SBO
// iterator handle: tk_iterator_t must still be exactly 40 bytes on 64-bit
// (8-byte vtable pointer + 32-byte state buffer). If a future change adds a
// state field and pushes the size past 40, this typedef fails to compile.
typedef char tk_iterator_t_must_stay_40_bytes[(sizeof(tk_iterator_t) == 40) ? 1
                                                                           : -1];

// --- vtable capability declarations ---

Test(iterator_random_access, vec_vtable_declares_random_access) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  int x = 0;
  tk_vec_push_back(v, &x);

  tk_iterator_t it = tk_vec_begin(v);
  cr_assert_not_null(it.vtable);

  // Category must be random access...
  cr_assert_eq(it.vtable->category, TK_ITER_RANDOM_ACCESS,
               "vec iterator must be TK_ITER_RANDOM_ACCESS");
  // ...and BOTH random-access slots must be populated.
  cr_assert_not_null(it.vtable->advance_by,
                     "random-access vtable must set advance_by");
  cr_assert_not_null(it.vtable->distance,
                     "random-access vtable must set distance");
  cr_assert_not_null(it.vtable->retreat,
                     "random-access vtable must also set retreat");

  // The vtable must pass the (extended) symmetric validation.
  tk_iterator_vtable_validate(it.vtable);

  tk_vec_destroy(v);
}

Test(iterator_random_access, list_vtable_does_not_declare_random_access) {
  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  int x = 0;
  tk_list_push_back(l, &x);

  tk_iterator_t it = tk_list_begin(l);
  cr_assert_not_null(it.vtable);

  cr_assert_eq(it.vtable->category, TK_ITER_BIDIRECTIONAL,
               "list iterator must be TK_ITER_BIDIRECTIONAL");
  // A bidirectional iterator must NOT advertise random access.
  cr_assert_null(it.vtable->advance_by,
                 "bidirectional vtable must leave advance_by NULL");
  cr_assert_null(it.vtable->distance,
                 "bidirectional vtable must leave distance NULL");
  cr_assert_not_null(it.vtable->retreat,
                     "bidirectional vtable must set retreat");

  tk_iterator_vtable_validate(it.vtable);

  tk_list_destroy(l);
}

// --- advance_by ---

Test(iterator_random_access, vec_advance_by_forward) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 0; i < 5; ++i) {
    int val = (i + 1) * 10; // {10,20,30,40,50}
    tk_vec_push_back(v, &val);
  }

  tk_iterator_t it = tk_vec_begin(v);
  tk_iter_advance_by(&it, 2); // begin + 2 -> element index 2 (value 30)
  cr_assert_eq(*(int *)tk_iter_get(&it), 30,
               "advance_by(+2) from begin should reach the 3rd element");

  // Advancing to end() must make it compare equal to end().
  tk_iter_advance_by(&it, 3); // now one past the last element
  tk_iterator_t end = tk_vec_end(v);
  cr_assert(tk_iter_equal(&it, &end) == true,
            "advance_by to the end boundary must equal end()");

  tk_vec_destroy(v);
}

Test(iterator_random_access, vec_advance_by_backward) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 0; i < 5; ++i) {
    int val = (i + 1) * 10; // {10,20,30,40,50}
    tk_vec_push_back(v, &val);
  }

  // From end(), move back 2 elements -> element index 3 (value 40).
  tk_iterator_t it = tk_vec_end(v);
  tk_iter_advance_by(&it, -2);
  cr_assert_eq(*(int *)tk_iter_get(&it), 40,
               "advance_by(-2) from end should reach the 4th element");

  // And back to begin().
  tk_iter_advance_by(&it, -3);
  tk_iterator_t begin = tk_vec_begin(v);
  cr_assert(tk_iter_equal(&it, &begin) == true,
            "advance_by back to begin must equal begin()");

  tk_vec_destroy(v);
}

Test(iterator_random_access, vec_advance_by_negative_clamps_at_begin) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  int a = 1;
  tk_vec_push_back(v, &a);

  tk_iterator_t it = tk_vec_begin(v);
  // A wildly underflowing negative move must clamp at begin(), never step out
  // of bounds (mirrors the retreat contract).
  tk_iter_advance_by(&it, -100);
  tk_iterator_t begin = tk_vec_begin(v);
  cr_assert(tk_iter_equal(&it, &begin) == true,
            "advance_by with an underflowing n must clamp at begin()");

  tk_vec_destroy(v);
}

Test(iterator_random_access, vec_advance_by_zero_is_noop) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  int a = 7;
  tk_vec_push_back(v, &a);

  tk_iterator_t it = tk_vec_begin(v);
  tk_iter_advance_by(&it, 0);
  tk_iterator_t begin = tk_vec_begin(v);
  cr_assert(tk_iter_equal(&it, &begin) == true,
            "advance_by(0) must be a no-op");

  tk_vec_destroy(v);
}

// --- distance ---

Test(iterator_random_access, vec_distance_sign_and_value) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 0; i < 5; ++i) {
    int val = i;
    tk_vec_push_back(v, &val);
  }

  tk_iterator_t begin = tk_vec_begin(v);
  tk_iterator_t end = tk_vec_end(v);

  // distance(begin, end) == +N (element count, NOT byte count).
  cr_assert_eq(tk_iter_distance(&begin, &end), (ptrdiff_t)5,
               "distance(begin,end) must be the element count (+5)");

  // distance(end, begin) == -N (signed).
  cr_assert_eq(tk_iter_distance(&end, &begin), (ptrdiff_t)-5,
               "distance(end,begin) must be negative (-5)");

  // distance(x, x) == 0.
  cr_assert_eq(tk_iter_distance(&begin, &begin), (ptrdiff_t)0,
               "distance of an iterator with itself must be 0");

  tk_vec_destroy(v);
}

Test(iterator_random_access, vec_distance_after_advance_by) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  for (int i = 0; i < 6; ++i) {
    int val = i;
    tk_vec_push_back(v, &val);
  }

  tk_iterator_t begin = tk_vec_begin(v);
  tk_iterator_t mid = tk_vec_begin(v);
  tk_iter_advance_by(&mid, 4);

  cr_assert_eq(tk_iter_distance(&begin, &mid), (ptrdiff_t)4,
               "distance(begin, begin+4) must be +4");
  cr_assert_eq(tk_iter_distance(&mid, &begin), (ptrdiff_t)-4,
               "distance(begin+4, begin) must be -4");

  tk_vec_destroy(v);
}

// --- Invalid-iterator safety of the wrappers ---

Test(iterator_random_access, wrappers_are_safe_on_invalid_iterator) {
  tk_iterator_t inv = {.vtable = NULL};
  tk_iterator_t inv2 = {.vtable = NULL};

  // advance_by: no-op, must not crash or revive the iterator.
  tk_iter_advance_by(&inv, 3);
  tk_iter_advance_by(&inv, -3);
  cr_assert_null(inv.vtable, "advance_by must not revive an invalid iterator");

  // distance: 0 sentinel when either operand is invalid.
  cr_assert_eq(tk_iter_distance(&inv, &inv2), (ptrdiff_t)0,
               "distance of invalid iterators must be 0");
  cr_assert_eq(tk_iter_distance(&inv, &inv), (ptrdiff_t)0,
               "distance of an invalid iterator with itself must be 0");
}

// --- Container types are distinguishable at the protocol level ---

Test(iterator_random_access, container_types_have_distinct_vtables) {
  tk_vec_t *v = tk_vec_create(sizeof(int));
  cr_assert_not_null(v);
  int a = 1;
  tk_vec_push_back(v, &a);

  tk_list_t *l = tk_list_create(sizeof(int));
  cr_assert_not_null(l);
  tk_list_push_back(l, &a);

  tk_iterator_t vit = tk_vec_begin(v);
  tk_iterator_t lit = tk_list_begin(l);

  // The two containers must have distinct vtables and categories. (Note:
  // calling tk_iter_distance across distinct vtables is a *caller violation*
  // and is asserted in debug builds, so it is deliberately NOT exercised here;
  // the release-time sentinel of 0 is covered by the invalid-iterator test.)
  cr_assert(vit.vtable != lit.vtable,
            "vec and list iterators must have different vtables");
  cr_assert(vit.vtable->category != lit.vtable->category,
            "vec (random access) and list (bidirectional) categories differ");

  tk_vec_destroy(v);
  tk_list_destroy(l);
}

// ===========================================================================
// Negative tests for tk_iterator_vtable_validate (QA B1).
//
// tk_iterator_vtable_validate uses TK_ASSERT, which calls abort(). Criterion
// runs each test in its own forked process, so calling the validator directly
// with an illegal vtable would abort *the test process* and be reported as a
// CRASH (which can never pass). We therefore fork a grandchild: the grandchild
// calls the validator and the test process inspects the grandchild's exit
// status. The vtable stubs below are self-contained (they reference no real
// container symbols).
// ===========================================================================
#ifndef NDEBUG // TK_ASSERT is a no-op under NDEBUG, so these only make sense in debug.

static void stub_advance(tk_iterator_t *self) { (void)self; }
static void *stub_get(const tk_iterator_t *self) {
  (void)self;
  return NULL;
}
static tk_bool stub_equal(const tk_iterator_t *a, const tk_iterator_t *b) {
  (void)a;
  (void)b;
  return false;
}
static void stub_clone(tk_iterator_t *dest, const tk_iterator_t *src) {
  (void)dest;
  (void)src;
}
static void stub_retreat(tk_iterator_t *self) { (void)self; }
static void stub_advance_by(tk_iterator_t *self, ptrdiff_t n) {
  (void)self;
  (void)n;
}
static ptrdiff_t stub_distance(const tk_iterator_t *a,
                               const tk_iterator_t *b) {
  (void)a;
  (void)b;
  return 0;
}

// Illegal: RANDOM_ACCESS, but the advance_by slot is missing.
static const tk_iterator_vtable_t g_bad_ra_no_advance_by = {
    .category = TK_ITER_RANDOM_ACCESS,
    .type_name = "bad_ra_no_advance_by",
    .advance = stub_advance,
    .get = stub_get,
    .equal = stub_equal,
    .clone = stub_clone,
    .retreat = stub_retreat,
    .advance_by = NULL,
    .distance = stub_distance};

// Illegal: RANDOM_ACCESS, but the distance slot is missing.
static const tk_iterator_vtable_t g_bad_ra_no_distance = {
    .category = TK_ITER_RANDOM_ACCESS,
    .type_name = "bad_ra_no_distance",
    .advance = stub_advance,
    .get = stub_get,
    .equal = stub_equal,
    .clone = stub_clone,
    .retreat = stub_retreat,
    .advance_by = stub_advance_by,
    .distance = NULL};

// Illegal: FORWARD (forward-only), but the random-access slots are present.
static const tk_iterator_vtable_t g_bad_forward_with_slots = {
    .category = TK_ITER_FORWARD,
    .type_name = "bad_forward_with_slots",
    .advance = stub_advance,
    .get = stub_get,
    .equal = stub_equal,
    .clone = stub_clone,
    .retreat = NULL,
    .advance_by = stub_advance_by,
    .distance = stub_distance};

// Legal random-access vtable (positive control).
static const tk_iterator_vtable_t g_good_ra = {
    .category = TK_ITER_RANDOM_ACCESS,
    .type_name = "good_ra",
    .advance = stub_advance,
    .get = stub_get,
    .equal = stub_equal,
    .clone = stub_clone,
    .retreat = stub_retreat,
    .advance_by = stub_advance_by,
    .distance = stub_distance};

// Runs the validator in a forked grandchild; true iff the grandchild was
// killed by SIGABRT (i.e. an assertion fired).
static tk_bool validate_aborts(const tk_iterator_vtable_t *vt) {
  pid_t pid = fork();
  cr_assert(pid >= 0, "fork() failed");
  if (pid == 0) {
    // Grandchild: if the validator returns, the vtable was accepted.
    tk_iterator_vtable_validate(vt);
    _exit(0);
  }
  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  cr_assert_eq(waited, pid, "waitpid() failed");
  return WIFSIGNALED(status) && (WTERMSIG(status) == SIGABRT);
}

// Runs the validator in a forked grandchild; true iff it exited normally with
// status 0 (i.e. the vtable was accepted).
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

Test(iterator_random_access, validate_rejects_ra_without_advance_by) {
  cr_assert(validate_aborts(&g_bad_ra_no_advance_by),
            "validator must abort for a RANDOM_ACCESS vtable whose advance_by "
            "slot is NULL");
}

Test(iterator_random_access, validate_rejects_ra_without_distance) {
  cr_assert(validate_aborts(&g_bad_ra_no_distance),
            "validator must abort for a RANDOM_ACCESS vtable whose distance "
            "slot is NULL");
}

Test(iterator_random_access, validate_rejects_forward_with_ra_slots) {
  cr_assert(validate_aborts(&g_bad_forward_with_slots),
            "validator must abort for a FORWARD vtable that advertises the "
            "random-access slots");
}

Test(iterator_random_access, validate_accepts_well_formed_ra) {
  // Positive control: a legal RA vtable must NOT abort (proves the negative
  // tests are not "any validator call crashes").
  cr_assert(validate_accepts(&g_good_ra),
            "validator must accept a well-formed RANDOM_ACCESS vtable");
}

#endif // !NDEBUG

// ===========================================================================
// element_size != sizeof(int) stepping (QA B2).
//
// The distance / advance_by tests above all use element_size == 4. These use
// 16-byte and 8-byte elements to pin down that distance is measured in
// *elements* (not bytes) and that advance_by steps whole elements -- exactly
// the class of bug a byte-vs-element confusion would introduce.
// ===========================================================================

typedef struct {
  int64_t a;
  int64_t b;
} pair16_t; // exactly 16 bytes (two fixed-width 64-bit fields)

Test(iterator_random_access, distance_and_advance_by_element_size_16) {
  cr_assert_eq(sizeof(pair16_t), (size_t)16, "pair16_t must be 16 bytes");

  tk_vec_t *v = tk_vec_create(sizeof(pair16_t));
  cr_assert_not_null(v);
  for (int64_t i = 0; i < 5; ++i) {
    pair16_t p;
    p.a = i;
    p.b = 100 + i;
    tk_vec_push_back(v, &p);
  }

  tk_iterator_t begin = tk_vec_begin(v);
  tk_iterator_t end = tk_vec_end(v);

  // distance is an element count: 5, NOT 80 (5 * 16 bytes).
  cr_assert_eq(tk_iter_distance(&begin, &end), (ptrdiff_t)5,
               "element_size=16: distance(begin,end) must be 5, not 80");
  cr_assert_eq(tk_iter_distance(&end, &begin), (ptrdiff_t)-5,
               "element_size=16: distance(end,begin) must be -5");

  tk_iterator_t it = tk_vec_begin(v);
  tk_iter_advance_by(&it, 3);
  const pair16_t *p = (const pair16_t *)tk_iter_get(&it);
  cr_assert_not_null(p);
  cr_assert_eq(p->a, (int64_t)3,
               "element_size=16: advance_by(3) must land on element[3]");
  cr_assert_eq(p->b, (int64_t)103,
               "element_size=16: element[3] payload must be intact");

  tk_iter_advance_by(&it, -2);
  p = (const pair16_t *)tk_iter_get(&it);
  cr_assert_eq(p->a, (int64_t)1,
               "element_size=16: advance_by(-2) must land on element[1]");

  tk_vec_destroy(v);
}

Test(iterator_random_access, distance_and_advance_by_element_size_8) {
  cr_assert_eq(sizeof(int64_t), (size_t)8, "int64_t must be 8 bytes");

  tk_vec_t *v = tk_vec_create(sizeof(int64_t));
  cr_assert_not_null(v);
  for (int64_t i = 0; i < 5; ++i) {
    tk_vec_push_back(v, &i);
  }

  tk_iterator_t begin = tk_vec_begin(v);
  tk_iterator_t end = tk_vec_end(v);

  cr_assert_eq(tk_iter_distance(&begin, &end), (ptrdiff_t)5,
               "element_size=8: distance(begin,end) must be 5, not 40");

  tk_iterator_t it = tk_vec_begin(v);
  tk_iter_advance_by(&it, 3);
  const int64_t *val = (const int64_t *)tk_iter_get(&it);
  cr_assert_not_null(val);
  cr_assert_eq(*val, (int64_t)3,
               "element_size=8: advance_by(3) must land on element[3]");

  tk_iter_advance_by(&it, -2);
  val = (const int64_t *)tk_iter_get(&it);
  cr_assert_eq(*val, (int64_t)1,
               "element_size=8: advance_by(-2) must land on element[1]");

  tk_vec_destroy(v);
}
