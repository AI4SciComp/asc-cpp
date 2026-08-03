#ifndef ASC_UTILITIES_EXPORT_H_
#define ASC_UTILITIES_EXPORT_H_

/**
 * @file
 * @brief Public Utilities declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_utilities
 */

#if defined(_WIN32)
#if defined(ASC_UTILITIES_STATIC_DEFINE)
#define ASC_UTILITIES_EXPORT
#elif defined(ASC_UTILITIES_BUILDING_LIBRARY)
#define ASC_UTILITIES_EXPORT __declspec(dllexport)
#else
#define ASC_UTILITIES_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASC_UTILITIES_EXPORT __attribute__((visibility("default")))
#else
/**
 * @brief Controls the public ASC_UTILITIES_EXPORT declaration or contract
 * behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Utilities module contract.
 * @ingroup asc_utilities
 */
#define ASC_UTILITIES_EXPORT
#endif

#endif  // ASC_UTILITIES_EXPORT_H_
