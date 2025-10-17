/**
 * @file types.h
 * @brief Defines the fundamental and unified data types for the entire toolkit.
 *
 * This header is the foundational stone of the core module. It ensures that all
 * other modules use a consistent and explicit set of basic types for sizes,
 * boolean logic, and integer widths.
 */
#ifndef TOOLKIT_TYPES_H
#define TOOLKIT_TYPES_H

#include <stdbool.h> // For the standard 'bool' type (requires C99 or later)
#include <stddef.h>  // For size_t and NULL
#include <stdint.h>  // For fixed-width integer types like int32_t, uint64_t

/**
 * @brief The standard boolean type for the toolkit.
 *
 * All functions returning a truth value should use this type for clarity
 * and type safety instead of returning an 'int'.
 */
typedef bool tk_bool;

#endif // TOOLKIT_TYPES_H
