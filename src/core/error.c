/**
 * @file error.c
 * @brief Implements the error handling utility functions for the toolkit.
 *
 * This file provides the concrete definitions for the functions declared in
 * error.h. Its primary responsibility is to translate the internal error codes
 * into human-readable strings, which is essential for effective debugging and
 * user-friendly error reporting.
 */

#include <tk/core/error.h>

/**
 * @brief Converts a toolkit error code into a human-readable, constant string.
 *
 * This function maps each enumerator from the tk_error_t type to a
 * corresponding null-terminated string literal that describes the error. It is
 * designed to be used in logging, debugging, or any scenario where a textual
 * representation of an error is required.
 *
 * The returned string is a constant literal and must not be modified by the
 * caller.
 *
 * @param err The error code of type tk_error_t to be converted.
 * @return A pointer to a constant, null-terminated string describing the
 * error. If the error code is not recognized, a generic "Unknown error"
 * string is returned.
 */
const char *tk_strerror(tk_error_t err) {
  // Use a switch statement for efficient and clear mapping of error codes.
  switch (err) {
  // --- Success Case ---
  case TK_SUCCESS:
    return "Success";

  // --- General Errors ---
  case TK_E_NOMEM:
    return "Out of memory";
  case TK_E_INVALID_ARG:
    return "Invalid argument";

  // --- Container Specific Errors ---
  case TK_E_OUT_OF_BOUNDS:
    return "Access out of bounds";
  case TK_E_EMPTY:
    return "Container is empty";
  case TK_E_NOT_FOUND:
    return "Item not found";

  // --- Default Case ---
  case TK_E_UNKNOWN:
  default:
    // Handle any unrecognized or future error codes gracefully.
    return "Unknown error";
  }
}
