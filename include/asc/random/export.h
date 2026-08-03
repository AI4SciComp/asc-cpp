#ifndef ASC_RANDOM_EXPORT_H_
#define ASC_RANDOM_EXPORT_H_

/**
 * @file
 * @brief Public Random declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_random
 */

#if defined(_WIN32)
#if defined(ASC_RANDOM_STATIC_DEFINE)
#define ASC_RANDOM_EXPORT
#elif defined(ASC_RANDOM_BUILDING_LIBRARY)
#define ASC_RANDOM_EXPORT __declspec(dllexport)
#else
#define ASC_RANDOM_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASC_RANDOM_EXPORT __attribute__((visibility("default")))
#else
/**
 * @brief Controls the public ASC_RANDOM_EXPORT declaration or contract
 * behavior.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random
 */
#define ASC_RANDOM_EXPORT
#endif

#endif  // ASC_RANDOM_EXPORT_H_
