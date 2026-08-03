#ifndef ASC_SPARSE_EXPORT_H_
#define ASC_SPARSE_EXPORT_H_

/**
 * @file
 * @brief Public Sparse declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_sparse
 */

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(ASC_SPARSE_STATIC_DEFINE)
#define ASC_SPARSE_EXPORT
#elif defined(asc_sparse_EXPORTS)
#define ASC_SPARSE_EXPORT __declspec(dllexport)
#else
#define ASC_SPARSE_EXPORT __declspec(dllimport)
#endif
#else
#if defined(asc_sparse_EXPORTS)
#define ASC_SPARSE_EXPORT __attribute__((visibility("default")))
#else
/**
 * @brief Controls the public ASC_SPARSE_EXPORT declaration or contract
 * behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Sparse module contract.
 * @ingroup asc_sparse
 */
#define ASC_SPARSE_EXPORT
#endif
#endif

#endif  // ASC_SPARSE_EXPORT_H_
