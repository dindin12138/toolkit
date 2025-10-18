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
 * ... (TK_ASSERT a a a a ... )
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
#define container_of(ptr, type, member)                                        \
  ({                                                                           \
    const typeof(((type *)0)->member) *__mptr = (ptr);                         \
    (type *)((char *)__mptr - offsetof(type, member));                         \
  })
#else
// Portable version for other compilers (e.g., MSVC), lacks type checking
#define container_of(ptr, type, member)                                        \
  ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

#endif // TOOLKIT_CORE_MACROS_H
