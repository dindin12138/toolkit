/**
 * @file macros.h
 * @brief Provides a set of common, highly-reused utility macros for the
 * toolkit.
 */
#ifndef TOOLKIT_CORE_MACROS_H
#define TOOLKIT_CORE_MACROS_H

#include <tk/core/types.h> // For size_t via <stddef.h>

/**
 * @brief A robust assertion macro for debugging.
 *
 * Expands to `assert(expr)` in debug builds and to a no-op when `NDEBUG` is
 * defined. It is intended for *internal invariants* only; public API entry
 * points should validate their arguments explicitly and return a sentinel or
 * error code so that behaviour is well-defined even in release builds.
 *
 * @param expr The expression that must hold.
 */
#ifdef NDEBUG
#define TK_ASSERT(expr) ((void)0)
#else
#include <assert.h>
#define TK_ASSERT(expr) assert(expr)
#endif

/**
 * @brief Casts a pointer to a struct member back to a pointer to its containing
 * struct.
 *
 * This is a utility macro intended for future intrusive containers; it is not
 * used by the current non-intrusive containers. It is provided (and prefixed
 * with `tk_` to comply with the toolkit naming convention) so that upcoming
 * intrusive node types can reuse it.
 *
 * This version is inspired by the modern Linux kernel implementation, providing
 * compile-time type checking when using GCC or Clang. It ensures that the
 * pointer to the member has the same type as the member itself, preventing
 * subtle bugs.
 *
 * @param ptr The pointer to the member.
 * @param type The type of the container struct.
 * @param member The name of the member within the struct.
 * @return A pointer of type 'type *' to the containing struct.
 */
#if defined(__GNUC__) || defined(__clang__)
// GCC/Clang version with compile-time type checking
#define tk_container_of(ptr, type, member)                                     \
  ({                                                                           \
    const typeof(((type *)0)->member) *__mptr = (ptr);                         \
    (type *)((char *)__mptr - offsetof(type, member));                         \
  })
#else
// Portable version for other compilers (e.g., MSVC), lacks type checking
#define tk_container_of(ptr, type, member)                                     \
  ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

#endif // TOOLKIT_CORE_MACROS_H
