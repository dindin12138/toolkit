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

#include <tk/core/types.h>

// Forward-declare the main iterator struct so the vtable can reference it.
typedef struct tk_iterator_t tk_iterator_t;

/**
 * @brief The "Iterator Protocol" virtual table (vtable).
 *
 * Each container (like tk_vec_t) must implement this interface
 * for its specific iterators.
 */
typedef struct {
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

} tk_iterator_vtable_t;

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

#endif // TOOLKIT_CORE_ITERATOR_H
