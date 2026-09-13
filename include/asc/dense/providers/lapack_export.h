#ifndef ASC_DENSE_PROVIDERS_LAPACK_EXPORT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_EXPORT_H_

/** @file
 * @brief Visibility for the optional Dense-owned reference provider facet.
 */

/** @brief Exports provider symbols only; never changes base Dense linkage. */
#if defined(_WIN32)
#if defined(ASC_DENSE_LAPACK_STATIC_DEFINE)
#define ASC_DENSE_LAPACK_EXPORT
#elif defined(ASC_DENSE_LAPACK_BUILDING_LIBRARY)
#define ASC_DENSE_LAPACK_EXPORT __declspec(dllexport)
#else
#define ASC_DENSE_LAPACK_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASC_DENSE_LAPACK_EXPORT __attribute__((visibility("default")))
#else
#define ASC_DENSE_LAPACK_EXPORT
#endif

#endif  // ASC_DENSE_PROVIDERS_LAPACK_EXPORT_H_
