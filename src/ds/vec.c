/**
 * @file vec.c
 * @brief Implements the public interface for the toolkit's generic dynamic
 * array (vector).
 *
 * @details
 * This implementation serves as a type-safe, element-oriented wrapper around
 * Sean Barrett's stb_ds.h library.
 *
 * The core design strategy is to force stb_ds to operate in a "byte mode" by
 * using a `char*` as the internal array type. Each function in this public
 * API then acts as a "translator," converting the user's element-based
 * requests (e.g., size in elements) into the byte-based operations that
 * stb_ds expects. This encapsulation is critical for providing a generic API
 * while preventing the memory corruption bugs that arise from misinterpreting
 * the unit of length or capacity.
 */

#include <stdlib.h>
#include <string.h>
#include <tk/core/iterator.h>
#include <tk/core/macros.h>
#include <tk/ds/vec.h>

#define STB_DS_IMPLEMENTATION
#include <stb/stb_ds.h>

/**
 * @struct tk_vec_t
 * @brief The opaque struct for the dynamic array (vector).
 */
struct tk_vec_t {
  /**
   * @brief The internal stb_ds dynamic array.
   * @details Stored as a `char*` to force all stb_ds macros (arrlen, arrcap)
   * to operate on a byte-level, which is the key to our generic
   * implementation.
   */
  char *stb_array;

  /**
   * @brief The size of a single element in bytes.
   * @details This is the "translation factor" used in almost every function
   * to convert between the public element count and the internal byte count.
   */
  size_t element_size;
};

// --- Lifecycle Functions ---

tk_vec_t *tk_vec_create(size_t element_size) {
  TK_ASSERT(element_size > 0);
  if (element_size == 0)
    return NULL;

  tk_vec_t *vec = (tk_vec_t *)malloc(sizeof(tk_vec_t));
  if (!vec)
    return NULL;

  // Initialize the internal array to NULL, as expected by stb_ds.
  vec->stb_array = NULL;
  vec->element_size = element_size;
  return vec;
}

void tk_vec_destroy(tk_vec_t *vec) {
  if (!vec)
    return;

  // arrfree safely handles the internal stb_ds array.
  arrfree(vec->stb_array);
  free(vec);
}

// --- Capacity Functions ---

size_t tk_vec_size(const tk_vec_t *vec) {
  TK_ASSERT(vec);
  // Translation: stb_ds's arrlenu returns the length in bytes. We convert
  // it to the number of elements for the public API.
  return arrlenu(vec->stb_array) / vec->element_size;
}

tk_bool tk_vec_is_empty(const tk_vec_t *vec) {
  TK_ASSERT(vec);
  // This function is correct because it relies on our translated tk_vec_size.
  return tk_vec_size(vec) == 0;
}

size_t tk_vec_capacity(const tk_vec_t *vec) {
  TK_ASSERT(vec);
  // Translation: stb_ds's arrcap returns the capacity in bytes. We convert
  // it to the number of elements.
  return arrcap(vec->stb_array) / vec->element_size;
}

tk_error_t tk_vec_reserve(tk_vec_t *vec, size_t n) {
  TK_ASSERT(vec);
  // Translation: The user requests capacity for `n` elements. We must ask
  // stb_ds for `n * element_size` bytes of capacity.
  arrsetcap(vec->stb_array, n * vec->element_size);
  if (n > 0 && tk_vec_capacity(vec) < n) {
    return TK_E_NOMEM;
  }
  return TK_SUCCESS;
}

// --- Element Access Functions ---
// Note: These functions are inherently correct because their byte-offset
// arithmetic aligns perfectly with our `char*` internal storage.

void *tk_vec_at(const tk_vec_t *vec, size_t index) {
  TK_ASSERT(vec);
  if (index >= tk_vec_size(vec))
    return NULL;
  return vec->stb_array + (index * vec->element_size);
}

void *tk_vec_front(const tk_vec_t *vec) {
  TK_ASSERT(vec);
  if (tk_vec_is_empty(vec))
    return NULL;
  return vec->stb_array;
}

void *tk_vec_back(const tk_vec_t *vec) {
  TK_ASSERT(vec);
  if (tk_vec_is_empty(vec))
    return NULL;
  size_t last_index = tk_vec_size(vec) - 1;
  return vec->stb_array + (last_index * vec->element_size);
}

// --- Modifiers ---

tk_error_t tk_vec_push_back(tk_vec_t *vec, const void *element) {
  TK_ASSERT(vec && element);

  // This is the safest and most critical part of the implementation.
  // We use `arraddnptr` to grow the byte array by `element_size` bytes.
  // This macro correctly handles all internal logic for reallocation and
  // updating the byte-length in the stb_ds header, completely avoiding the
  // memory corruption bugs caused by manual header manipulation.
  void *dest = arraddnptr(vec->stb_array, vec->element_size);

  // In the current stb_ds implementation, arraddnptr on failure returns the
  // original pointer without growing capacity. A capacity check is the most
  // reliable way to detect allocation failure.
  if (tk_vec_capacity(vec) * vec->element_size < arrlenu(vec->stb_array)) {
    return TK_E_NOMEM;
  }

  // Now it's safe to copy the user's data into the new space.
  memcpy(dest, element, vec->element_size);
  return TK_SUCCESS;
}

void tk_vec_pop_back(tk_vec_t *vec) {
  TK_ASSERT(vec);
  if (!tk_vec_is_empty(vec)) {
    // Translation: To pop one element, we must reduce the internal byte
    // length by the size of one element.
    size_t new_len_in_bytes = arrlenu(vec->stb_array) - vec->element_size;
    arrsetlen(vec->stb_array, new_len_in_bytes);
  }
}

void tk_vec_clear(tk_vec_t *vec) {
  TK_ASSERT(vec);
  // This is inherently correct, as setting the byte-length to 0 clears the
  // vector regardless of element size.
  arrsetlen(vec->stb_array, 0);
}

// --- Iterator Implementation ---

/**
 * @brief Private state for a tk_vec_t iterator.
 *
 * This struct fits within the 32-byte SBO buffer of tk_iterator_t.
 */
typedef struct {
  char *ptr;           // Pointer to the current element (8 bytes)
  size_t element_size; // Size of one element (8 bytes)
} tk_vec_iter_state_t;

// --- vtable function implementations ---

/**
 * @brief (vtable) Advances the vector iterator to the next element.
 */
static void tk_vec_iter_advance(tk_iterator_t *self) {
  // Get the private state from the SBO buffer
  tk_vec_iter_state_t *state = (tk_vec_iter_state_t *)self->state.data;
  // Advance the pointer by one element size
  state->ptr += state->element_size;
}

/**
 * @brief (vtable) Gets the data pointer from the vector iterator.
 */
static void *tk_vec_iter_get(const tk_iterator_t *self) {
  // Get the private state
  const tk_vec_iter_state_t *state =
      (const tk_vec_iter_state_t *)self->state.data;
  // Return the pointer to the data
  return state->ptr;
}

/**
 * @brief (vtable) Checks if two vector iterators are equal.
 */
static tk_bool tk_vec_iter_equal(const tk_iterator_t *iter1,
                                 const tk_iterator_t *iter2) {
  // Get the private state for both iterators
  const tk_vec_iter_state_t *state1 =
      (const tk_vec_iter_state_t *)iter1->state.data;
  const tk_vec_iter_state_t *state2 =
      (const tk_vec_iter_state_t *)iter2->state.data;
  // They are equal if their data pointers are the same.
  //    (We assume element_size is the same since the vtable is the same,
  //     which is already checked by the public tk_iter_equal).
  return state1->ptr == state2->ptr;
}

/**
 * @brief (vtable) Clones a vector iterator.
 */
static void tk_vec_iter_clone(tk_iterator_t *dest, const tk_iterator_t *src) {
  // The simplest, fastest way to clone is to copy the entire struct.
  // This copies both the vtable pointer and the 32-byte state union.
  *dest = *src;
}

/**
 * @brief The single, static vtable for all tk_vec_t iterators.
 *
 * This uses the TK_DEFINE_ITERATOR_VTABLE macro to ensure all
 * function pointers and metadata fields are correctly initialized.
 */
static const tk_iterator_vtable_t g_vec_vtable =
    TK_DEFINE_ITERATOR_VTABLE(tk_vec_iter,           /* Function Prefix */
                              TK_ITER_RANDOM_ACCESS, /* Category */
                              "tk_vec_iterator");    /* Type Name */

// --- Public iterator function implementations ---

tk_iterator_t tk_vec_begin(tk_vec_t *vec) {
  TK_ASSERT(vec);
  // Validate the vtable in debug builds.
  tk_iterator_vtable_validate(&g_vec_vtable);

  tk_iterator_t iter;
  // Point to the correct "instruction manual" (vtable)
  iter.vtable = &g_vec_vtable;

  // Get the pointer to the internal state buffer
  tk_vec_iter_state_t *state = (tk_vec_iter_state_t *)iter.state.data;

  // Fill the state
  state->element_size = vec->element_size;
  state->ptr = vec->stb_array; // 'stb_array' points to the first element

  return iter;
}

tk_iterator_t tk_vec_end(tk_vec_t *vec) {
  TK_ASSERT(vec);
  tk_iterator_t iter;
  // Point to the correct vtable
  iter.vtable = &g_vec_vtable;

  // Get the state buffer pointer
  tk_vec_iter_state_t *state = (tk_vec_iter_state_t *)iter.state.data;

  // Fill the state
  state->element_size = vec->element_size;
  // The "end" iterator points *past* the last element.
  state->ptr = vec->stb_array + (tk_vec_size(vec) * vec->element_size);

  return iter;
}
