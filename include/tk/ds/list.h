/**
 * @file list.h
 * @brief Public interface for the toolkit's generic doubly-linked list.
 *
 * This file defines the API for a type-safe, non-intrusive, generic
 * doubly-linked list container. It follows the opaque pointer pattern
 * established by tk_vec_t.
 *
 * Core semantics:
 * - Non-intrusive: The list manages its own nodes internally.
 * - Stores copies: Like tk_vec_t, it allocates memory for and copies the
 * user's element data using memcpy.
 * - Runtime polymorphic: Integrates with the tk_iterator_t system.
 */
#ifndef TOOLKIT_DS_LIST_H
#define TOOLKIT_DS_LIST_H

#include <tk/core/error.h>    //
#include <tk/core/iterator.h> //
#include <tk/core/types.h>    //

// Forward declaration of the opaque structure
typedef struct tk_list_t tk_list_t;

// Re-use the element destroyer type definition
typedef void (*tk_element_destroyer_t)(void *element_ptr);

// --- Lifecycle Functions ---
// Mimics tk_vec_create, tk_vec_destroy, tk_vec_destroy_full

/**
 * @brief Creates a new list instance.
 * @param element_size The size in bytes of each element to be stored.
 * @return A pointer to the new list, or NULL if memory allocation fails.
 */
tk_list_t *tk_list_create(size_t element_size);

/**
 * @brief Destroys a list instance and frees all associated memory (nodes).
 * Performs a "shallow" destroy, meaning it does not free the user data
 * stored within the list unless an element destroyer is used via
 * tk_list_destroy_full.
 * @param list A pointer to the list handle to be destroyed. If NULL, the
 * function does nothing.
 */
void tk_list_destroy(tk_list_t *list);

/**
 * @brief Destroys a list and deep-frees its elements using a custom destroyer.
 * This should be used when list elements themselves own resources (like
 * pointers). It iterates over every element and calls the provided `destroyer`
 * function on a pointer *to* the element data before freeing the node itself.
 * @param list A pointer to the list handle to be destroyed.
 * @param destroyer A function pointer that will be called for each element's
 * data to free its resources. If NULL, this functions behaves identically
 * to `tk_list_destroy`.
 */
void tk_list_destroy_full(tk_list_t *list, tk_element_destroyer_t destroyer);

// --- Size/Query Functions ---
// Mimics tk_vec_size, tk_vec_is_empty
// Capacity/reserve are not typically used for linked lists.

/**
 * @brief Returns the number of elements in the list. O(1) complexity.
 * @param list A constant pointer to the list handle.
 * @return The number of elements.
 */
size_t tk_list_size(const tk_list_t *list);

/**
 * @brief Checks if the list is empty. O(1) complexity.
 * @param list A constant pointer to the list handle.
 * @return `true` if the list size is 0, `false` otherwise.
 */
tk_bool tk_list_is_empty(const tk_list_t *list);

// --- Element Access Functions ---
// Mimics tk_vec_front, tk_vec_back
// `tk_list_at` (by index) is O(n) and generally discouraged for lists.

/**
 * @brief Returns a pointer to the first element's data in the list. O(1).
 * @param list A constant pointer to the list handle.
 * @return A pointer to the first element's data, or NULL if the list is empty.
 *
 */
const void *tk_list_front(const tk_list_t *list);

/**
 * @brief Returns a mutable pointer to the first element's data in the list.
 * O(1).
 * @param list A pointer to the list handle.
 * @return A mutable pointer to the first element's data, or NULL if the list is
 * empty.
 */
void *tk_list_front_mut(tk_list_t *list);

/**
 * @brief Returns a pointer to the last element's data in the list. O(1).
 * @param list A constant pointer to the list handle.
 * @return A pointer to the last element's data, or NULL if the list is empty.
 *
 */
const void *tk_list_back(const tk_list_t *list);

/**
 * @brief Returns a mutable pointer to the last element's data in the list.
 * O(1).
 * @param list A pointer to the list handle.
 * @return A mutable pointer to the last element's data, or NULL if the list is
 * empty.
 */
void *tk_list_back_mut(tk_list_t *list);

// --- Modifiers ---
// Mimics tk_vec_push_back, tk_vec_pop_back, tk_vec_clear
// Adds list-specific front operations.

/**
 * @brief Adds an element to the end of the list. O(1).
 * @param list A pointer to the list handle.
 * @param element A pointer to the element data to be copied into the list.
 *
 * @return TK_SUCCESS on success, TK_E_NOMEM if node or data allocation fails.
 *
 */
tk_error_t tk_list_push_back(tk_list_t *list, const void *element);

/**
 * @brief Removes the last element from the list. O(1). Does nothing if empty.
 * @param list A pointer to the list handle.
 */
void tk_list_pop_back(tk_list_t *list);

/**
 * @brief Adds an element to the beginning of the list. O(1).
 * @param list A pointer to the list handle.
 * @param element A pointer to the element data to be copied into the list.
 * @return TK_SUCCESS on success, TK_E_NOMEM if allocation fails.
 */
tk_error_t tk_list_push_front(tk_list_t *list, const void *element);

/**
 * @brief Removes the first element from the list. O(1). Does nothing if empty.
 * @param list A pointer to the list handle.
 */
void tk_list_pop_front(tk_list_t *list);

/**
 * @brief Removes all elements from the list. O(n).
 * @param list A pointer to the list handle.
 */
void tk_list_clear(tk_list_t *list);

/**
 * @brief Inserts an element *before* the position indicated by the iterator.
 * O(1).
 * If `before_iter` is the end iterator, equivalent to `tk_list_push_back`.
 * @param list A pointer to the list handle.
 * @param before_iter An iterator pointing to the element before which insertion
 * should occur. Must be a valid iterator obtained from this
 * list (or the end iterator).
 * @param element A pointer to the element data to be copied.
 * @return TK_SUCCESS on success, TK_E_INVALID_ARG if iterator is invalid,
 * TK_E_NOMEM if allocation fails.
 */
tk_error_t tk_list_insert_before(tk_list_t *list, tk_iterator_t before_iter,
                                 const void *element);

/**
 * @brief Removes the element at the position indicated by the iterator. O(1).
 * The iterator `iter` becomes invalidated after this call.
 * @param list A pointer to the list handle.
 * @param iter An iterator pointing to the element to remove. Must be a valid,
 * dereferenceable iterator obtained from this list (not the end
 * iterator).
 * @return An iterator pointing to the element that followed the erased element,
 * or the end iterator if the last element was erased. Returns an
 * invalid iterator (vtable=NULL) on error (e.g., invalid input iter).
 */
tk_iterator_t tk_list_erase_at(tk_list_t *list, tk_iterator_t iter);

// --- Iterator Functions ---
// Mimics tk_vec_begin, tk_vec_end

/**
 * @brief Returns a polymorphic iterator pointing to the first element. O(1).
 * @param list A pointer to the list.
 * @return A `tk_iterator_t` pointing to the beginning.
 * If the list is empty, returns an iterator equal to `tk_list_end`.
 */
tk_iterator_t tk_list_begin(tk_list_t *list);

/**
 * @brief Returns a polymorphic iterator pointing past the last element. O(1).
 * @param list A pointer to the list.
 * @return A `tk_iterator_t` pointing to the end boundary.
 */
tk_iterator_t tk_list_end(tk_list_t *list);

#endif // TOOLKIT_DS_LIST_H
