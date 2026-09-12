/**
 * @file types.h
 * @brief Defines the fundamental and unified data types for the entire toolkit.
 *
 * This header is the foundational stone of the core module. It ensures that all
 * other modules use a consistent and explicit set of basic types for sizes,
 * boolean logic, and integer widths.
 */
#ifndef TOOLKIT_CORE_TYPES_H
#define TOOLKIT_CORE_TYPES_H

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

/**
 * @brief Signature of a user-supplied element destroyer.
 *
 * Receives a pointer TO the stored element (e.g. `char**` when the
 * container stores `char*`) and is responsible for releasing any
 * resources that element owns.
 *
 * @param element_ptr A pointer to the element within the container's storage.
 *
 * @note This is the single, canonical definition of the destroyer type for the
 * entire toolkit. Containers (tk_vec_t, tk_list_t, ...) MUST NOT redefine it:
 * under strict C99 a repeated typedef is a compile error, and a single source
 * of truth keeps the containers usable together in one translation unit.
 */
typedef void (*tk_element_destroyer_t)(void *element_ptr);

#endif // TOOLKIT_CORE_TYPES_H
