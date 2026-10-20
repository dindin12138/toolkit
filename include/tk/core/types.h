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

/**
 * @brief Signature of a user-supplied three-way comparator.
 *
 * Reports the ordering of two elements in a single call: negative when `a`
 * orders before `b`, zero when they are equivalent, positive when `a` orders
 * after `b`. This is the C standard library's `qsort` convention, not the
 * STL's two-way "is a less than b" predicate, because algorithms that reorder a
 * range must recognise equal elements (see ADR-0003). It is deliberately a
 * different type from the two-way predicates the sequence algorithms take: a
 * predicate answers a question about one element, a comparator orders two.
 *
 * The comparator must implement a strict weak ordering; violating that is the
 * caller's responsibility and makes the result undefined.
 *
 * @param a A pointer to the first element.
 * @param b A pointer to the second element.
 * @return A negative value, zero, or a positive value.
 *
 * @note Like tk_element_destroyer_t, this is the single canonical definition
 * for the whole toolkit; modules MUST NOT redefine it.
 */
typedef int (*tk_comparator_t)(const void *a, const void *b);

#endif // TOOLKIT_CORE_TYPES_H
