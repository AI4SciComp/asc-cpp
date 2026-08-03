#ifndef ASC_DENSE_EXPORT_H_
#define ASC_DENSE_EXPORT_H_

/**
 * @file
 * @brief Public Dense declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_dense
 */

#if defined(_WIN32)
#if defined(ASC_DENSE_STATIC_DEFINE)
#define ASC_DENSE_EXPORT
#elif defined(ASC_DENSE_BUILDING_LIBRARY)
#define ASC_DENSE_EXPORT __declspec(dllexport)
#else
#define ASC_DENSE_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASC_DENSE_EXPORT __attribute__((visibility("default")))
#else
/**
 * @brief Controls the public ASC_DENSE_EXPORT declaration or contract behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Dense module contract.
 * @ingroup asc_dense
 */
#define ASC_DENSE_EXPORT
#endif

#endif  // ASC_DENSE_EXPORT_H_
