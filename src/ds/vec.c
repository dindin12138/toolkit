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

#include <stdint.h> // For SIZE_MAX
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

/**
 * @brief Implements the deep-free destroyer function.
 */
void tk_vec_destroy_full(tk_vec_t *vec, tk_element_destroyer_t destroyer) {
  if (!vec)
    return;

  // If a destroyer is provided, iterate and call it for each
  // element.
  if (destroyer) {
    size_t size = tk_vec_size(vec);
    for (size_t i = 0; i < size; ++i) {
      // tk_vec_at_mut returns a pointer TO the element in the array. We use the
      // mutable variant here because the destroyer may free resources owned by
      // the element (and `vec` is non-const in this function).
      void *element_ptr = tk_vec_at_mut(vec, i);
      if (element_ptr) {
        destroyer(element_ptr);
      }
    }
  }

  // Now, safely free the vector's internal array and the struct itself.
  arrfree(vec->stb_array);
  free(vec);
}

// --- Capacity Functions ---

size_t tk_vec_size(const tk_vec_t *vec) {
  if (!vec)
    return 0;
  // Translation: stb_ds's arrlenu returns the length in bytes. We convert
  // it to the number of elements for the public API.
  return arrlenu(vec->stb_array) / vec->element_size;
}

tk_bool tk_vec_is_empty(const tk_vec_t *vec) {
  // This function is correct because it relies on our translated tk_vec_size.
  return tk_vec_size(vec) == 0;
}

size_t tk_vec_capacity(const tk_vec_t *vec) {
  if (!vec)
    return 0;
  // Translation: stb_ds's arrcap returns the capacity in bytes. We convert
  // it to the number of elements.
  return arrcap(vec->stb_array) / vec->element_size;
}

tk_error_t tk_vec_reserve(tk_vec_t *vec, size_t n) {
  if (!vec)
    return TK_E_INVALID_ARG;

  // Guard against `n * element_size` overflowing size_t before we ask stb_ds
  // to grow the array.
  if (vec->element_size != 0 && n > SIZE_MAX / vec->element_size)
    return TK_E_NOMEM;

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
//
// The read and mutable forms share a single internal helper so the bounds
// logic cannot drift between them. The helper returns a plain `char *`:
// accessed through a `const tk_vec_t *`, `vec->stb_array` has type
// `char *const` -- the POINTER is const but the pointed-to chars are still
// writable, so `stb_array + offset` is already `char *`. No const is cast
// away anywhere below.

/**
 * @brief Internal: the byte address of element `index`, or NULL.
 *
 * Returns NULL for a NULL handle, an empty vector or an out-of-bounds index.
 * The returned pointer addresses the vector's independent element storage,
 * which is not part of the const-qualified handle object itself.
 *
 * @param vec A constant pointer to the vector handle.
 * @param index The element index.
 * @return A `char *` to the element's bytes, or NULL if out of bounds.
 */
static char *tk_vec_element_ptr(const tk_vec_t *vec, size_t index) {
  if (!vec || index >= tk_vec_size(vec))
    return NULL;
  return vec->stb_array + (index * vec->element_size);
}

const void *tk_vec_at(const tk_vec_t *vec, size_t index) {
  return tk_vec_element_ptr(vec, index); // char* -> const void* (adds const)
}

void *tk_vec_at_mut(tk_vec_t *vec, size_t index) {
  return tk_vec_element_ptr(vec, index); // char* -> void* (compatible)
}

const void *tk_vec_front(const tk_vec_t *vec) {
  return tk_vec_element_ptr(vec, 0);
}

void *tk_vec_front_mut(tk_vec_t *vec) { return tk_vec_element_ptr(vec, 0); }

const void *tk_vec_back(const tk_vec_t *vec) {
  if (!vec || tk_vec_is_empty(vec))
    return NULL;
  return tk_vec_element_ptr(vec, tk_vec_size(vec) - 1);
}

void *tk_vec_back_mut(tk_vec_t *vec) {
  if (!vec || tk_vec_is_empty(vec))
    return NULL;
  return tk_vec_element_ptr(vec, tk_vec_size(vec) - 1);
}

// --- Modifiers ---

tk_error_t tk_vec_push_back(tk_vec_t *vec, const void *element) {
  if (!vec || !element)
    return TK_E_INVALID_ARG;

  // This is the safest and most critical part of the implementation.
  // We use `arraddnptr` to grow the byte array by `element_size` bytes.
  // This macro correctly handles all internal logic for reallocation and
  // updating the byte-length in the stb_ds header, completely avoiding the
  // memory corruption bugs caused by manual header manipulation.
  void *dest = arraddnptr(vec->stb_array, vec->element_size);

  // In the current stb_ds implementation, arraddnptr on failure returns the
  // original pointer without growing capacity. A capacity check is the most
  // reliable way to detect allocation failure. We compare byte counts directly
  // (arrcap/arrlenu are both in bytes), which avoids the integer overflow that
  // `capacity * element_size` would risk.
  if (arrcap(vec->stb_array) < arrlenu(vec->stb_array)) {
    return TK_E_NOMEM;
  }

  // Now it's safe to copy the user's data into the new space.
  memcpy(dest, element, vec->element_size);
  return TK_SUCCESS;
}

void tk_vec_pop_back(tk_vec_t *vec) {
  if (!vec || tk_vec_is_empty(vec))
    return;
  // Translation: To pop one element, we must reduce the internal byte
  // length by the size of one element.
  size_t new_len_in_bytes = arrlenu(vec->stb_array) - vec->element_size;
  arrsetlen(vec->stb_array, new_len_in_bytes);
}

void tk_vec_clear(tk_vec_t *vec) {
  if (!vec)
    return;
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
  char *begin;         // Lower bound (first element), for retreat clamping
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
 * @brief (vtable) Retreats the vector iterator to the previous element.
 * Required for TK_ITER_RANDOM_ACCESS >= TK_ITER_BIDIRECTIONAL.
 *
 * Retreating before begin() is a documented no-op (stays at begin()); it is
 * clamped against the recorded lower bound so that it can never step out of
 * bounds.
 */
static void tk_vec_iter_retreat(tk_iterator_t *self) {
  // Get the private state from the SBO buffer
  tk_vec_iter_state_t *state = (tk_vec_iter_state_t *)self->state.data;
  // Retreating before begin() is a documented no-op (stays at begin()).
  if (state->ptr > state->begin) {
    state->ptr -= state->element_size;
  }
}

/**
 * @brief (vtable) Moves the vector iterator by `n` elements.
 *
 * Required for TK_ITER_RANDOM_ACCESS. This is O(1): a single pointer
 * adjustment of `n * element_size` bytes.
 *
 * Contract (see iterator.h): moving past end() is a caller precondition
 * violation and is not detectable here. A negative `n` that would move before
 * begin() is clamped at begin(), mirroring the retreat contract, so an
 * underflowing move is a well-defined no-op rather than undefined behaviour.
 */
static void tk_vec_iter_advance_by(tk_iterator_t *self, ptrdiff_t n) {
  tk_vec_iter_state_t *state = (tk_vec_iter_state_t *)self->state.data;

  if (n < 0) {
    // Number of elements currently between begin() and the current position.
    ptrdiff_t back =
        (state->ptr - state->begin) / (ptrdiff_t)state->element_size;
    // Clamp at begin(): never step out of bounds on the low side.
    if (-n > back)
      n = -back;
  }

  // `n` and element_size are both ptrdiff_t here, so the product is a signed
  // byte offset and no sign-conversion warning is emitted.
  state->ptr += n * (ptrdiff_t)state->element_size;
}

/**
 * @brief (vtable) Returns the signed element distance from `a` to `b`.
 *
 * Required for TK_ITER_RANDOM_ACCESS. Returns (index of b) - (index of a) in
 * *elements* (not bytes); the byte difference is divided by element_size.
 *
 * The `a->vtable == b->vtable` check is an internal invariant asserted by the
 * tk_iter_distance wrapper (a caller violation); it is re-checked here as a
 * cheap internal invariant.
 */
static ptrdiff_t tk_vec_iter_distance(const tk_iterator_t *a,
                                      const tk_iterator_t *b) {
  const tk_vec_iter_state_t *sa =
      (const tk_vec_iter_state_t *)a->state.data;
  const tk_vec_iter_state_t *sb =
      (const tk_vec_iter_state_t *)b->state.data;
  TK_ASSERT(a->vtable == b->vtable);
  // Return element count, not bytes: divide the byte delta by element_size.
  return (ptrdiff_t)((sb->ptr - sa->ptr) / (ptrdiff_t)sa->element_size);
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
  // This copies the vtable pointer and the whole 32-byte state union
  // (including the `begin` lower bound).
  *dest = *src;
}

/**
 * @brief The single, static vtable for all tk_vec_t iterators.
 *
 * This uses the TK_DEFINE_RANDOM_ACCESS_ITERATOR_VTABLE macro, which populates
 * the `retreat`, `advance_by` and `distance` slots required by the
 * TK_ITER_RANDOM_ACCESS category.
 */
static const tk_iterator_vtable_t g_vec_vtable =
    TK_DEFINE_RANDOM_ACCESS_ITERATOR_VTABLE(tk_vec_iter, /* Function Prefix */
                                            "tk_vec_iterator"); /* Type Name */

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
  state->begin = vec->stb_array; // Lower bound for retreat clamping
  state->ptr = vec->stb_array;   // 'stb_array' points to the first element

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
  state->begin = vec->stb_array; // Lower bound for retreat clamping
  // The "end" iterator points *past* the last element.
  state->ptr = vec->stb_array + (tk_vec_size(vec) * vec->element_size);

  return iter;
}
