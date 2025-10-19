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
   * MUST be implemented if category is TK_ITER_BIDIRECTIONAL or
   * TK_ITER_RANDOM_ACCESS. Can be NULL otherwise.
   * @param self A pointer to the iterator to be retreated.
   */
  void (*retreat)(tk_iterator_t *self); // <-- Add this line

} tk_iterator_vtable_t;

/**
 * @brief A macro to safely and consistently define an
 * iterator vtable.
 *
 * This ensures all function pointers and metadata fields are set,
 * preventing incomplete or inconsistent vtable definitions as the
 * interface evolves.
 *
 * @param PREFIX The unique prefix for the iterator's static functions
 * (e.g., `tk_vec_iter`).
 * @param CATEGORY The `tk_iter_category_t` for this iterator
 * (e.g., `TK_ITER_RANDOM_ACCESS`).
 * @param TYPENAME A string literal for this iterator's type
 * (e.g., "tk_vec_iterator").
 */
#define TK_DEFINE_ITERATOR_VTABLE(PREFIX, CATEGORY, TYPENAME)                  \
  {.category = (CATEGORY),                                                     \
   .type_name = (TYPENAME),                                                    \
   .advance = PREFIX##_advance,                                                \
   .get = PREFIX##_get,                                                        \
   .equal = PREFIX##_equal,                                                    \
   .clone = PREFIX##_clone,                                                    \
   .retreat = ((CATEGORY) >= TK_ITER_BIDIRECTIONAL) ? PREFIX##_retreat : NULL}

/**
 * @brief The unified, polymorphic iterator type.
 *
 * This struct is the "handle" that all generic algorithms will use.
 * It is intentionally designed to be small-buffer optimized (SBO).
 * Its size (e.g., 32 bytes on 64-bit) is large enough to hold the
 * state of most common iterators (like a pointer + a size) directly
 * within its own memory, avoiding the need for extra heap allocations
 * for the iterator's state.
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
 * non-NULL.
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
  TK_ASSERT((vtable->category < TK_ITER_BIDIRECTIONAL) ||
            (vtable->retreat != NULL));
}

/**
 * @brief Advances the iterator to the next element.
 * (Calls the vtable's 'advance' function).
 * @param iter A pointer to the iterator to advance.
 */
static inline void tk_iter_next(tk_iterator_t *iter) {
  iter->vtable->advance(iter);
}

/**
 * @brief Gets a pointer to the element the iterator points to.
 * (Calls the vtable's 'get' function).
 * @param iter A constant pointer to the iterator.
 * @return A `void*` pointer to the user's data element.
 */
static inline void *tk_iter_get(const tk_iterator_t *iter) {
  return iter->vtable->get(iter);
}

/**
 * @brief Checks if two iterators are equal.
 * (Calls the vtable's 'equal' function).
 * @param iter1 A constant pointer to the first iterator.
 * @param iter2 A constant pointer to the second iterator.
 * @return `true` if they are equal, `false` otherwise.
 */
static inline tk_bool tk_iter_equal(const tk_iterator_t *iter1,
                                    const tk_iterator_t *iter2) {
  // Iterators can only be equal if they are of the same type
  // (i.e., share the same vtable) and their vtable's equal func says so.
  return (iter1->vtable == iter2->vtable) &&
         (iter1->vtable->equal(iter1, iter2));
}

/**
 * @brief Creates a copy of an iterator.
 * (Calls the vtable's 'clone' function).
 * @param dest A pointer to the destination iterator (will be overwritten).
 * @param src A constant pointer to the source iterator to copy.
 */
static inline void tk_iter_clone(tk_iterator_t *dest,
                                 const tk_iterator_t *src) {
  src->vtable->clone(dest, src);
}

/**
 * @brief Retreats the iterator to the previous element.
 * (Calls the vtable's 'retreat' function).
 * Asserts that the iterator is at least bidirectional.
 * @param iter A pointer to the iterator to retreat.
 */
static inline void tk_iter_prev(tk_iterator_t *iter) {
  TK_ASSERT(iter->vtable->category >=
            TK_ITER_BIDIRECTIONAL);         // Ensure capability
  TK_ASSERT(iter->vtable->retreat != NULL); // Ensure function exists
  iter->vtable->retreat(iter);
}

#endif // TOOLKIT_CORE_ITERATOR_H
