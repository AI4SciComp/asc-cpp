#ifndef ASC_CORE_EXPORT_H_
#define ASC_CORE_EXPORT_H_

/**
 * @file
 * @brief Public Core declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_core
 */

#if defined(_WIN32)
#if defined(ASC_CORE_STATIC_DEFINE)
#define ASC_CORE_EXPORT
#elif defined(ASC_CORE_BUILDING_LIBRARY)
#define ASC_CORE_EXPORT __declspec(dllexport)
#else
#define ASC_CORE_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASC_CORE_EXPORT __attribute__((visibility("default")))
#else
/**
 * @brief Controls the public ASC_CORE_EXPORT declaration or contract behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
#define ASC_CORE_EXPORT
#endif

#endif  // ASC_CORE_EXPORT_H_
