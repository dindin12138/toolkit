/**
 * @file vec.h
 * @brief Public interface for the toolkit's generic dynamic array (vector).
 *
 * This file defines the API for a type-safe, generic dynamic array container.
 * It is implemented using an opaque pointer (tk_vec_t) to hide the underlying
 * implementation details, providing a stable and consistent interface.
 *
 * Ownership model
 * ---------------
 * A tk_vec_t stores *byte copies* of your elements (it copies `element_size`
 * bytes with memcpy). Therefore:
 *  - tk_vec_pop_back and tk_vec_clear are SHALLOW: they drop the stored bytes
 *    but do NOT release resources owned by those elements.
 *  - tk_vec_destroy_full is the DEEP operation: it invokes your destroyer for
 *    every element before freeing storage.
 * If an element owns heap memory (e.g. the vector stores `char*`), release it
 * yourself before pop/clear, or destroy the vector with tk_vec_destroy_full.
 */
#ifndef TOOLKIT_DS_VEC_H
#define TOOLKIT_DS_VEC_H

#include <tk/core/error.h>
#include <tk/core/iterator.h>
#include <tk/core/types.h>

// Forward declaration of the opaque structure. The user never knows its
// contents.
typedef struct tk_vec_t tk_vec_t;

// The tk_element_destroyer_t type is defined once, in <tk/core/types.h>
// (included above), and is shared by all containers.

// --- Lifecycle Functions ---

/**
 * @brief Creates a new vector instance.
 * @param element_size The size in bytes of each element to be stored in the
 * vector. Must be greater than 0.
 * @return A pointer to the new vector, or NULL if memory allocation fails or
 * `element_size` is 0.
 */
tk_vec_t *tk_vec_create(size_t element_size);

/**
 * @brief Destroys a vector instance and frees all associated memory.
 * @param vec A pointer to the vector handle to be destroyed. If NULL, the
 * function does nothing.
 */
void tk_vec_destroy(tk_vec_t *vec);

/**
 * @brief Destroys a vector and deep-frees its elements using a
 * custom destroyer.
 *
 * This function should be used when the vector stores pointers or other
 * resources that need to be manually freed. It iterates over every element
 * and calls the provided `destroyer` function on it before freeing the
 * vector itself.
 *
 * @param vec A pointer to the vector handle to be destroyed. If NULL, the
 * function does nothing.
 * @param destroyer A function pointer that will be called for each element
 * to free its resources. If NULL, this functions behaves
 * identically to `tk_vec_destroy`.
 */
void tk_vec_destroy_full(tk_vec_t *vec, tk_element_destroyer_t destroyer);

// --- Capacity Functions ---

/**
 * @brief Returns the number of elements in the vector.
 * @param vec A constant pointer to the vector handle. If NULL, returns 0.
 * @return The number of elements.
 */
size_t tk_vec_size(const tk_vec_t *vec);

/**
 * @brief Checks if the vector is empty.
 * @param vec A constant pointer to the vector handle. If NULL, returns `true`.
 * @return `true` if the vector size is 0, `false` otherwise.
 */
tk_bool tk_vec_is_empty(const tk_vec_t *vec);

/**
 * @brief Returns the total number of elements the vector can hold before
 * needing to reallocate.
 * @param vec A constant pointer to the vector handle. If NULL, returns 0.
 * @return The current capacity.
 */
size_t tk_vec_capacity(const tk_vec_t *vec);

/**
 * @brief Requests that the vector capacity be at least enough to contain n
 * elements.
 * @param vec A pointer to the vector handle. If NULL, returns
 * TK_E_INVALID_ARG.
 * @param n The new capacity.
 * @return TK_SUCCESS on success, TK_E_INVALID_ARG if `vec` is NULL, or
 * TK_E_NOMEM if `n * element_size` would overflow `size_t` or reallocation
 * fails.
 *
 * @warning Allocation failure inside the underlying stb_ds layer is not
 * gracefully recoverable (stb_ds does not check its realloc result). The
 * TK_E_NOMEM return therefore only covers the pre-checkable overflow case; a
 * true out-of-memory condition may abort instead.
 */
tk_error_t tk_vec_reserve(tk_vec_t *vec, size_t n);

// --- Element Access Functions ---

/**
 * @brief Returns a pointer to the element at the specified index, with bounds
 * checking.
 * @param vec A constant pointer to the vector handle. If NULL, returns NULL.
 * @param index The index of the element to access.
 * @return A pointer to the element, or NULL if the index is out of bounds.
 */
void *tk_vec_at(const tk_vec_t *vec, size_t index);

/**
 * @brief Returns a pointer to the first element in the vector.
 * @param vec A constant pointer to the vector handle. If NULL, returns NULL.
 * @return A pointer to the first element, or NULL if the vector is empty.
 */
void *tk_vec_front(const tk_vec_t *vec);

/**
 * @brief Returns a pointer to the last element in the vector.
 * @param vec A constant pointer to the vector handle. If NULL, returns NULL.
 * @return A pointer to the last element, or NULL if the vector is empty.
 */
void *tk_vec_back(const tk_vec_t *vec);

// --- Modifiers ---

/**
 * @brief Adds an element to the end of the vector.
 * @param vec A pointer to the vector handle. If NULL, returns
 * TK_E_INVALID_ARG.
 * @param element A pointer to the element to be copied into the vector. If
 * NULL, returns TK_E_INVALID_ARG.
 * @return TK_SUCCESS on success, TK_E_INVALID_ARG if an argument is NULL, or
 * TK_E_NOMEM on allocation failure.
 *
 * @warning Allocation failure inside the underlying stb_ds layer is not
 * gracefully recoverable (stb_ds does not check its realloc result). The
 * TK_E_NOMEM return therefore only covers the pre-checkable overflow case; a
 * true out-of-memory condition may abort instead.
 */
tk_error_t tk_vec_push_back(tk_vec_t *vec, const void *element);

/**
 * @brief Removes the last element from the vector.
 * @param vec A pointer to the vector handle. If NULL or empty, does nothing.
 *
 * @note SHALLOW operation: it does not release resources owned by the removed
 * element. See the ownership model note at the top of this file.
 */
void tk_vec_pop_back(tk_vec_t *vec);

/**
 * @brief Removes all elements from the vector, leaving the capacity unchanged.
 * @param vec A pointer to the vector handle. If NULL, does nothing.
 *
 * @note SHALLOW operation: it does not release resources owned by the removed
 * elements. See the ownership model note at the top of this file.
 */
void tk_vec_clear(tk_vec_t *vec);

// --- Iterator Functions ---

/**
 * @brief Returns a polymorphic iterator pointing to the first element.
 * @param vec A pointer to the vector.
 * @return A `tk_iterator_t` pointing to the beginning of the vector.
 */
tk_iterator_t tk_vec_begin(tk_vec_t *vec);

/**
 * @brief Returns a polymorphic iterator pointing past the last element.
 * @param vec A pointer to the vector.
 * @return A `tk_iterator_t` pointing to the end boundary of the vector.
 */
tk_iterator_t tk_vec_end(tk_vec_t *vec);

#endif // TOOLKIT_DS_VEC_H
