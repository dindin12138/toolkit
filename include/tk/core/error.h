/**
 * @file error.h
 * @brief Defines the standard error codes and error handling mechanisms for the
 * toolkit.
 *
 * Error-reporting conventions used throughout the toolkit:
 *  - Operations that can fail (allocation, bounds-checked mutation) return a
 *    `tk_error_t`. Any out-parameter is written only on TK_SUCCESS.
 *  - Pure query / accessor functions that never allocate return a sentinel:
 *    NULL means "no element / out of bounds / invalid", while 0 or false are
 *    returned for sizes and predicates. Each function documents its sentinel.
 *  - `tk_list_erase_at` returns an *invalid iterator* (vtable == NULL) on
 *    error; all `tk_iter_*` helpers are NULL-vtable safe (see iterator.h).
 *
 * A return value of TK_SUCCESS (which is 0) indicates success, while any
 * non-zero value indicates an error.
 */
#ifndef TOOLKIT_CORE_ERROR_H
#define TOOLKIT_CORE_ERROR_H

#include <tk/core/types.h>

/**
 * @brief Defines the standard error codes for the toolkit.
 */
typedef enum {
  /**
   * @brief The operation completed successfully.
   */
  TK_SUCCESS = 0,

  // --- General Errors ---
  /**
   * @brief A memory allocation failed (e.g., malloc, realloc returned NULL).
   * This is a critical error, often unrecoverable.
   */
  TK_E_NOMEM,

  /**
   * @brief An invalid argument was provided to a function (e.g., a NULL
   * pointer).
   */
  TK_E_INVALID_ARG,

  /**
   * @brief An unknown or unspecified error occurred.
   */
  TK_E_UNKNOWN,

  // --- Container Specific Errors ---
  /**
   * @brief An access attempt was made outside the valid bounds of a container.
   * (e.g., accessing index 10 in a vector of size 5).
   */
  TK_E_OUT_OF_BOUNDS,

  /**
   * @brief An operation was attempted on an empty container that is not
   * allowed. (e.g., pop_back on an empty vector).
   */
  TK_E_EMPTY,

  /**
   * @brief A requested item was not found.
   * (e.g., getting a key from a hash map that does not exist).
   */
  TK_E_NOT_FOUND,

} tk_error_t;

/**
 * @brief Converts a toolkit error code to a human-readable, constant string.
 *
 * This function is useful for logging and debugging purposes.
 * The returned string should not be modified.
 *
 * @param err The error code of type tk_error_t.
 * @return A constant string describing the error. Returns "Unknown Error" if
 * the error code is not recognized.
 */
const char *tk_strerror(tk_error_t err);

#endif // TOOLKIT_CORE_ERROR_H
