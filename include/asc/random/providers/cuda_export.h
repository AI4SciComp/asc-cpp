#ifndef ASC_RANDOM_PROVIDERS_CUDA_EXPORT_H_
#define ASC_RANDOM_PROVIDERS_CUDA_EXPORT_H_

/**
 * @file
 * @brief Public CUDA provider declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_cuda
 */

#if defined(_WIN32)
#if defined(ASC_RANDOM_CUDA_STATIC_DEFINE)
#define ASC_RANDOM_CUDA_EXPORT
#elif defined(ASC_RANDOM_CUDA_BUILDING_LIBRARY)
#define ASC_RANDOM_CUDA_EXPORT __declspec(dllexport)
#else
#define ASC_RANDOM_CUDA_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASC_RANDOM_CUDA_EXPORT __attribute__((visibility("default")))
#else
/**
 * @brief Controls the public ASC_RANDOM_CUDA_EXPORT declaration or contract
 * behavior.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
#define ASC_RANDOM_CUDA_EXPORT
#endif

#endif  // ASC_RANDOM_PROVIDERS_CUDA_EXPORT_H_
