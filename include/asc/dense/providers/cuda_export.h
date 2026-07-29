#ifndef ASC_DENSE_PROVIDERS_CUDA_EXPORT_H_
#define ASC_DENSE_PROVIDERS_CUDA_EXPORT_H_

#if defined(_WIN32)
#if defined(ASC_DENSE_CUDA_STATIC_DEFINE)
#define ASC_DENSE_CUDA_EXPORT
#elif defined(ASC_DENSE_CUDA_BUILDING_LIBRARY)
#define ASC_DENSE_CUDA_EXPORT __declspec(dllexport)
#else
#define ASC_DENSE_CUDA_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASC_DENSE_CUDA_EXPORT __attribute__((visibility("default")))
#else
#define ASC_DENSE_CUDA_EXPORT
#endif

#endif  // ASC_DENSE_PROVIDERS_CUDA_EXPORT_H_
