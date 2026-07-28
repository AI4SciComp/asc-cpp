#ifndef ASC_CORE_EXPORT_H_
#define ASC_CORE_EXPORT_H_

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
#define ASC_CORE_EXPORT
#endif

#endif  // ASC_CORE_EXPORT_H_
