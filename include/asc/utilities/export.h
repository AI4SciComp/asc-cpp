#ifndef ASC_UTILITIES_EXPORT_H_
#define ASC_UTILITIES_EXPORT_H_

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
#define ASC_UTILITIES_EXPORT
#endif

#endif  // ASC_UTILITIES_EXPORT_H_
