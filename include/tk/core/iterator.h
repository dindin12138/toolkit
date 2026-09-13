/**
 * @file iterator.h
 * @brief Defines the core polymorphic iterator interface for the toolkit.
 *
 * @details
 * This file defines the "iterator protocol" for all non-intrusive containers
 * in the toolkit. It uses a "vtable" (virtual table) approach, which is a
 * struct of function pointers, to achieve runtime polymorphism in C.
 *
 * Any generic algorithm (like sort, find_if, etc.) will operate on the
 * `tk_iterator_t` type. This iterator is a "fat" iterator; it carries both
 * its own private state and a pointer to the vtable that knows how to
 * operate on that state.
 *
 * This design allows a single `tk_algo_find` function to work identically on
 * a `tk_vec_t` (which advances by pointer arithmetic) and a future `tk_list_t`
 * (which advances by following a 'next' pointer), without the algorithm
 * needing to know the difference.
 *
 * @section iter_invalidation Iterator invalidation rules
 * - Any operation that may reallocate or re-link storage
 *   (tk_vec_push_back / tk_vec_reserve / tk_list_insert_before /
 *    tk_list_erase_at) invalidates *all* outstanding iterators of that
 *   container, including any previously obtained end() iterator.
 * - Retreating an iterator before begin() is a well-defined no-op: the vtable
 *   `retreat` implementations clamp at begin() instead of stepping out of
 *   bounds (vec) or silently jumping to end() (list).
 * - An invalid iterator (vtable == NULL, returned by tk_list_erase_at on
 *   error) must not be dereferenced; the tk_iter_* helpers below treat it as a
 *   no-op (next/prev), return NULL (get) or report "not equal to anything"
 *   (equal).
 */
#ifndef TOOLKIT_CORE_ITERATOR_H
#define TOOLKIT_CORE_ITERATOR_H

#include <tk/core/macros.h>
#include <tk/core/types.h>

// Forward-declare the main iterator struct so the vtable can reference it.
typedef struct tk_iterator_t tk_iterator_t;

/**
 * @brief Defines the category (capabilities) of an iterator.
 *
 * This allows algorithms to assert that they have the required
 * capabilities (e.g., sorting requires random access).
 */
typedef enum {
  /**
   * @brief Can move forward, one element at a time.
   */
  TK_ITER_FORWARD,
  /**
   * @brief Can move forward and backward.
   */
  TK_ITER_BIDIRECTIONAL,
  /**
   * @brief Can be accessed at any offset in O(1). (e.g., tk_vec_t)
   */
  TK_ITER_RANDOM_ACCESS
} tk_iter_category_t;

/**
 * @brief The "Iterator Protocol" virtual table (vtable).
 *
 * Each container (like tk_vec_t) must implement this interface
 * for its specific iterators.
 */
typedef struct {
  /**
   * @brief The capability category of this iterator.
   */
  tk_iter_category_t category;

  /**
   * @brief A unique string identifier for the iterator type.
   * Used in debug builds to assert type safety.
   */
  const char *type_name;

  /**
   * @brief Advances the iterator 'self' to the next element.
   * @param self A pointer to the iterator to be advanced.
   */
  void (*advance)(tk_iterator_t *self);

  /**
   * @brief Returns a pointer to the data element the iterator 'self'
   * currently points to.
   * @param self A constant pointer to the iterator.
   * @return A `void*` pointer to the user's data element.
   */
  void *(*get)(const tk_iterator_t *self);

  /**
   * @brief Compares two iterators for equality.
   * Assumes both iterators are of the same concrete type.
   * @param iter1 A constant pointer to the first iterator.
   * @param iter2 A constant pointer to the second iterator.
   * @return `true` if they point to the same element, `false` otherwise.
   */
  tk_bool (*equal)(const tk_iterator_t *iter1, const tk_iterator_t *iter2);

  /**
   * @brief Clones the state of 'src' into 'dest'.
   * This is necessary for algorithms that need to copy iterators.
   * @param dest The destination iterator.
   * @param src The source iterator.
   */
  void (*clone)(tk_iterator_t *dest, const tk_iterator_t *src);

  /**
   * @brief (Optional) Retreats the iterator 'self' to the previous element.
   * MUST be non-NULL if category is TK_ITER_BIDIRECTIONAL or
   * TK_ITER_RANDOM_ACCESS. MUST be NULL for forward-only iterators
   * (TK_ITER_FORWARD); use TK_DEFINE_FORWARD_ITERATOR_VTABLE for those.
   * @param self A pointer to the iterator to be retreated.
   */
  void (*retreat)(tk_iterator_t *self);

  /* ---- Random-access slots (stage 1 / P1-1) ----
   * These two slots are populated ONLY by iterators whose category is
   * TK_ITER_RANDOM_ACCESS. They are the protocol-level expression of the
   * "random access" capability: a non-NULL slot is a promise of O(1) offset
   * movement / distance. They are appended at the END of the struct so that
   * pre-existing designated initializers keep compiling (unlisted trailing
   * fields are zero-initialized to NULL in C). */

  /**
   * @brief (Random access) Moves 'self' by `n` elements (n may be negative).
   *
   * MUST be non-NULL iff category == TK_ITER_RANDOM_ACCESS; MUST be NULL
   * otherwise (enforced by tk_iterator_vtable_validate).
   *
   * @pre `n` must lie within the representable stepping range
   * [-(pos - begin), end - pos] (where `pos` is the current position). Two
   * consequences are the caller's responsibility:
   *   - Moving *past* end() is a precondition violation that cannot be
   *     detected (iterators do not carry an upper bound).
   *   - The byte step `n * (ptrdiff_t)element_size` must be representable:
   *     for `n` near PTRDIFF_MAX with `element_size > 1` the product is
   *     signed-overflow UB.
   * Moving *before* begin() is defensively clamped at begin() (mirroring the
   * retreat contract), so an underflowing negative `n` is a well-defined
   * no-op -- except `n == PTRDIFF_MIN`, whose negation `-n` is itself
   * signed-overflow UB.
   *
   * @param self The iterator to move.
   * @param n The signed number of elements to move (positive = forward).
   */
  void (*advance_by)(tk_iterator_t *self, ptrdiff_t n);

  /**
   * @brief (Random access) Returns the signed number of steps from `a` to `b`,
   * i.e. (index of b) - (index of a).
   *
   * MUST be non-NULL iff category == TK_ITER_RANDOM_ACCESS; MUST be NULL
   * otherwise (enforced by tk_iterator_vtable_validate).
   *
   * @pre `a` and `b` must come from the SAME container instance. The protocol
   * can only check vtable identity, so two iterators of the same type but from
   * two DIFFERENT container instances cannot be detected and produce an
   * unspecified result. When the precondition holds, the result is positive
   * when `b` is ahead of `a`, negative when behind, and zero when they denote
   * the same position.
   *
   * @param a The "from" iterator.
   * @param b The "to" iterator.
   * @return The signed element distance from `a` to `b`.
   */
  ptrdiff_t (*distance)(const tk_iterator_t *a, const tk_iterator_t *b);

} tk_iterator_vtable_t;

/**
 * @brief A macro to safely and consistently define an iterator vtable.
 *
 * Use this for bidirectional iterators (those that provide a `PREFIX##_retreat`
 * function but no random-access slots). It ensures all function pointers and
 * metadata fields are set, preventing incomplete or inconsistent vtable
 * definitions as the interface evolves.
 *
 * The random-access slots are explicitly set to NULL: a bidirectional iterator
 * has no O(1) offset movement, and leaving these slots non-NULL would falsely
 * advertise random-access capability (see
 * TK_DEFINE_RANDOM_ACCESS_ITERATOR_VTABLE for the random-access variant).
 *
 * @param PREFIX The unique prefix for the iterator's static functions
 * (e.g., `tk_list_iter`). The prefix MUST define `_advance`, `_get`, `_equal`,
 * `_clone` and `_retreat`.
 * @param CATEGORY The `tk_iter_category_t` for this iterator. Must be
 * TK_ITER_BIDIRECTIONAL.
 * @param TYPENAME A string literal for this iterator's type
 * (e.g., "tk_list_iterator").
 */
#define TK_DEFINE_ITERATOR_VTABLE(PREFIX, CATEGORY, TYPENAME)                  \
  {.category = (CATEGORY),                                                     \
   .type_name = (TYPENAME),                                                    \
   .advance = PREFIX##_advance,                                                \
   .get = PREFIX##_get,                                                        \
   .equal = PREFIX##_equal,                                                    \
   .clone = PREFIX##_clone,                                                    \
   .retreat = PREFIX##_retreat,                                                \
   .advance_by = NULL,                                                         \
   .distance = NULL}

/**
 * @brief Defines a vtable for a forward-only iterator.
 *
 * A forward-only iterator has no `retreat` operation, so this macro sets
 * `.retreat = NULL` and does NOT reference a `PREFIX##_retreat` symbol. This
 * matters because the ternary expression used previously would still name the
 * missing symbol at compile time, breaking the build for any container that
 * (correctly) omits `_retreat`.
 *
 * The random-access slots are set to NULL as well: a forward iterator has
 * neither O(1) offset movement nor O(1) distance.
 *
 * @param PREFIX The unique prefix for the iterator's static functions. The
 * prefix MUST define `_advance`, `_get`, `_equal` and `_clone` (but no
 * `_retreat`).
 * @param TYPENAME A string literal for this iterator's type.
 */
#define TK_DEFINE_FORWARD_ITERATOR_VTABLE(PREFIX, TYPENAME)                    \
  {.category = TK_ITER_FORWARD,                                                \
   .type_name = (TYPENAME),                                                    \
   .advance = PREFIX##_advance,                                                \
   .get = PREFIX##_get,                                                        \
   .equal = PREFIX##_equal,                                                    \
   .clone = PREFIX##_clone,                                                    \
   .retreat = NULL,                                                            \
   .advance_by = NULL,                                                         \
   .distance = NULL}

/**
 * @brief Defines a vtable for a random-access iterator.
 *
 * A random-access iterator additionally provides O(1) offset movement and
 * distance, so this macro populates BOTH new slots by referencing
 * `PREFIX##_advance_by` and `PREFIX##_distance`. Using it keeps the
 * random-access capability declaration in lock-step with the implementation.
 *
 * @note Do NOT use TK_DEFINE_ITERATOR_VTABLE with TK_ITER_RANDOM_ACCESS: that
 * macro leaves the two random-access slots NULL, which the vtable validator
 * rejects for a random-access category.
 *
 * @param PREFIX The unique prefix for the iterator's static functions. The
 * prefix MUST define `_advance`, `_get`, `_equal`, `_clone`, `_retreat`,
 * `_advance_by` and `_distance`.
 * @param TYPENAME A string literal for this iterator's type.
 */
#define TK_DEFINE_RANDOM_ACCESS_ITERATOR_VTABLE(PREFIX, TYPENAME)              \
  {.category = TK_ITER_RANDOM_ACCESS,                                          \
   .type_name = (TYPENAME),                                                    \
   .advance = PREFIX##_advance,                                                \
   .get = PREFIX##_get,                                                        \
   .equal = PREFIX##_equal,                                                    \
   .clone = PREFIX##_clone,                                                    \
   .retreat = PREFIX##_retreat,                                                \
   .advance_by = PREFIX##_advance_by,                                          \
   .distance = PREFIX##_distance}

/**
 * @brief The unified, polymorphic iterator type.
 *
 * This struct is the "handle" that all generic algorithms will use.
 * It is intentionally designed to be small-buffer optimized (SBO).
 * Its size is 40 bytes on 64-bit (8-byte vtable pointer + 32-byte SBO
 * buffer), which is large enough to hold the state of most common iterators
 * (like a pointer + a size) directly within its own memory, avoiding the need
 * for extra heap allocations for the iterator's state.
 */
struct tk_iterator_t {
  /**
   * @brief Points to the vtable that implements the iterator protocol.
   */
  const tk_iterator_vtable_t *vtable;

  /**
   * @brief Internal state for the iterator.
   * For `tk_vec_t`, this might store { void* ptr, size_t element_size }.
   * For a future `tk_list_t`, this might store { struct tk_list_node_t* node }.
   * We use a union to provide 32 bytes of flexible, aligned storage.
   */
  union {
    void *pointers[4];
    size_t sizes[4];
    char data[32]; // 32 bytes of SBO storage
  } state;
};

// --- Generic Iterator Operations ---

/**
 * @brief Validates the completeness of a vtable in debug builds.
 *
 * Asserts that all essential function pointers and metadata fields are
 * non-NULL, that `retreat` is present if and only if the category is at
 * least bidirectional, and that the two random-access slots are present if
 * and only if the category is random-access.
 * @param vtable A pointer to the vtable to validate.
 */
static inline void
tk_iterator_vtable_validate(const tk_iterator_vtable_t *vtable) {
  (void)vtable; // Suppress unused warning in release builds
  TK_ASSERT(vtable != NULL);
  TK_ASSERT(vtable->advance != NULL);
  TK_ASSERT(vtable->get != NULL);
  TK_ASSERT(vtable->equal != NULL);
  TK_ASSERT(vtable->clone != NULL);
  TK_ASSERT(vtable->type_name != NULL);
  // Bidirectional/random-access iterators must provide retreat...
  TK_ASSERT((vtable->category < TK_ITER_BIDIRECTIONAL) ||
            (vtable->retreat != NULL));
  // ...and forward-only iterators must NOT provide one.
  TK_ASSERT((vtable->category >= TK_ITER_BIDIRECTIONAL) ||
            (vtable->retreat == NULL));
  // Random-access iterators must provide BOTH random-access slots...
  TK_ASSERT((vtable->category < TK_ITER_RANDOM_ACCESS) ||
            (vtable->advance_by != NULL && vtable->distance != NULL));
  // ...and non-random-access iterators must provide NEITHER of them.
  TK_ASSERT((vtable->category >= TK_ITER_RANDOM_ACCESS) ||
            (vtable->advance_by == NULL && vtable->distance == NULL));
}

/**
 * @brief Advances the iterator to the next element.
 * (Calls the vtable's 'advance' function).
 * An invalid iterator (vtable == NULL) is a no-op.
 * @param iter A pointer to the iterator to advance.
 */
static inline void tk_iter_next(tk_iterator_t *iter) {
  if (iter->vtable == NULL)
    return; // invalid iterator: no-op
  iter->vtable->advance(iter);
}

/**
 * @brief Gets a pointer to the element the iterator points to.
 * (Calls the vtable's 'get' function).
 * @param iter A constant pointer to the iterator.
 * @return A `void*` pointer to the user's data element, or NULL if the
 * iterator is invalid (vtable == NULL).
 */
static inline void *tk_iter_get(const tk_iterator_t *iter) {
  if (iter->vtable == NULL)
    return NULL; // invalid iterator: no element
  return iter->vtable->get(iter);
}

/**
 * @brief Checks if two iterators are equal.
 * (Calls the vtable's 'equal' function).
 *
 * Equality is only defined for iterators obtained from a container. An invalid
 * iterator (vtable == NULL) is never equal to anything, including another
 * invalid iterator.
 *
 * @param iter1 A constant pointer to the first iterator.
 * @param iter2 A constant pointer to the second iterator.
 * @return `true` if they are equal, `false` otherwise.
 */
static inline tk_bool tk_iter_equal(const tk_iterator_t *iter1,
                                    const tk_iterator_t *iter2) {
  // An invalid iterator is never equal to anything. This guard is essential:
  // without it, two NULL vtables would compare equal and we would call
  // NULL->equal, crashing.
  if (iter1->vtable == NULL || iter2->vtable == NULL)
    return false;
  // Iterators can only be equal if they are of the same type
  // (i.e., share the same vtable) and their vtable's equal func says so.
  return (iter1->vtable == iter2->vtable) &&
         (iter1->vtable->equal(iter1, iter2));
}

/**
 * @brief Creates a copy of an iterator.
 * (Calls the vtable's 'clone' function).
 * If the source is an invalid iterator (vtable == NULL), the destination is
 * marked invalid as well.
 * @param dest A pointer to the destination iterator (will be overwritten).
 * @param src A constant pointer to the source iterator to copy.
 */
static inline void tk_iter_clone(tk_iterator_t *dest,
                                 const tk_iterator_t *src) {
  if (src->vtable == NULL) {
    dest->vtable = NULL; // propagate the invalid marker
    return;
  }
  src->vtable->clone(dest, src);
}

/**
 * @brief Retreats the iterator to the previous element.
 * (Calls the vtable's 'retreat' function).
 * Asserts that the iterator is at least bidirectional.
 * An invalid iterator (vtable == NULL) is a no-op.
 * @param iter A pointer to the iterator to retreat.
 */
static inline void tk_iter_prev(tk_iterator_t *iter) {
  if (iter->vtable == NULL)
    return; // invalid iterator: no-op
  TK_ASSERT(iter->vtable->category >=
            TK_ITER_BIDIRECTIONAL);         // Ensure capability
  TK_ASSERT(iter->vtable->retreat != NULL); // Ensure function exists
  iter->vtable->retreat(iter);
}

/**
 * @brief (Random access) Moves the iterator by `n` elements.
 *
 * Only valid for random-access iterators; calling it on a forward/bidirectional
 * iterator is a caller precondition violation. In debug builds this is caught
 * by an assertion; in release builds (NDEBUG) the assertions are compiled out
 * and NO diagnostic is emitted -- the call then degrades to a silent no-op,
 * because a non-random-access vtable leaves its `advance_by` slot NULL. An
 * invalid iterator (vtable == NULL) is likewise a no-op.
 *
 * @pre The result must not move past end(); this cannot be detected. A
 * negative `n` that would move before begin() is clamped at begin(). Extreme
 * `n` (signed overflow of `n * element_size`) is a caller violation.
 *
 * @param iter A pointer to the iterator to move.
 * @param n The signed number of elements to move (positive = forward).
 */
static inline void tk_iter_advance_by(tk_iterator_t *iter, ptrdiff_t n) {
  if (iter->vtable == NULL)
    return; // invalid iterator: no-op
  TK_ASSERT(iter->vtable->category == TK_ITER_RANDOM_ACCESS &&
            "tk_iter_advance_by requires a RANDOM_ACCESS iterator");
  TK_ASSERT(iter->vtable->advance_by != NULL);
  if (iter->vtable->advance_by != NULL) // release-time defence: no-op if absent
    iter->vtable->advance_by(iter, n);
}

/**
 * @brief (Random access) Returns the signed number of steps from `a` to `b`,
 * i.e. (index of b) - (index of a).
 *
 * If either iterator is invalid (vtable == NULL), or the two iterators are not
 * comparable (different container types), or the iterator is not random-access,
 * this returns 0 as a sentinel (mirroring how tk_iter_equal reports "not
 * comparable" as false).
 *
 * In debug builds the caller-violation cases (mismatched vtables, non
 * random-access operands) are caught by assertions; in release builds (NDEBUG)
 * the assertions are compiled out and NO diagnostic is emitted -- the function
 * simply returns the 0 sentinel. Note the sentinel cannot be distinguished
 * from a genuine distance of 0.
 *
 * @param a The "from" iterator.
 * @param b The "to" iterator.
 * @return The signed element distance from `a` to `b`, or 0 when undefined.
 */
static inline ptrdiff_t tk_iter_distance(const tk_iterator_t *a,
                                         const tk_iterator_t *b) {
  if (a->vtable == NULL || b->vtable == NULL)
    return 0; // invalid: not comparable
  TK_ASSERT(a->vtable == b->vtable &&
            "tk_iter_distance: iterators from different container types");
  TK_ASSERT(a->vtable->category == TK_ITER_RANDOM_ACCESS &&
            "tk_iter_distance requires RANDOM_ACCESS iterators");
  TK_ASSERT(a->vtable->distance != NULL);
  if (a->vtable != b->vtable || a->vtable->distance == NULL)
    return 0; // release-time defence
  return a->vtable->distance(a, b);
}

#endif // TOOLKIT_CORE_ITERATOR_H
