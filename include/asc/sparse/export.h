#ifndef ASC_SPARSE_EXPORT_H_
#define ASC_SPARSE_EXPORT_H_

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
#define ASC_SPARSE_EXPORT
#endif
#endif

#endif  // ASC_SPARSE_EXPORT_H_
