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
