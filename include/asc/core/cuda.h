// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/cuda.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_CUDA_H_
#define ASC_CUDA_H_

/// @file cuda.h
/// @brief CUDA runtime wrappers and kernel execution macros
///
/// This module provides low-level CUDA functionality including:
/// - Memory allocation/deallocation (device, managed, pinned host)
/// - Memory transfer operations (host-to-device, device-to-host, device-to-device)
/// - Error checking macros and utilities
/// - Device function/kernel decorators
/// - Thread/block indexing macros for kernel code
///
/// @par Macro Overview:
/// - ASC_GPU_CHECK(x): Check CUDA error and abort on failure
/// - ASC_DEVICE: Mark function as device-only (__device__)
/// - ASC_HOST: Mark function as host-only (__host__)
/// - ASC_HOST_DEVICE: Mark function for both host and device
/// - ASC_DEVICE_SYNC: Synchronize device (cudaDeviceSynchronize)
/// - ASC_CUDA_BLOCKS: Default CUDA block size (256 threads)
///
/// @par Kernel Macros (inside __global__ kernels):
/// - ASC_SHARED: Declare shared memory variable
/// - ASC_SYNC_THREAD: Synchronize threads in block (__syncthreads)
/// - ASC_BLOCK_ID(k): Block index (blockIdx.k)
/// - ASC_THREAD_ID(k): Thread index (threadIdx.k)
/// - ASC_THREAD_SIZE(k): Block dimension (blockDim.k)
///
/// @note All functions are no-ops when ASC_USE_CUDA is not defined

#include "asc/core/config.h"
#include "asc/core/error.h"

// Only include CUDA headers when compiling with CUDA compiler (nvcc)
// When ASC_USE_CUDA is ON but we're compiling with a regular C++ compiler,
// we can skip the CUDA headers entirely to avoid compilation errors.
// #if defined(ASC_USE_CUDA) && defined(__CUDACC__)
#ifdef ASC_USE_CUDA
#include <cusparse.h>
#include <library_types.h>
#include <cuda_runtime.h>
#include <cuda.h>
#endif

// CUDA block size used by ASC.
#define ASC_CUDA_BLOCKS 256

#ifdef ASC_USE_CUDA
#define ASC_USE_CUDA_OR_HIP
#define ASC_DEVICE __device__
#define ASC_HOST __host__
#define ASC_LAMBDA __host__
// #define ASC_HOST_DEVICE __host__ __device__ // defined in config/config.hpp
#define ASC_DEVICE_SYNC ASC_GPU_CHECK(cudaDeviceSynchronize())
#define ASC_STREAM_SYNC ASC_GPU_CHECK(cudaStreamSynchronize(0))
// Define a CUDA error check macro, ASC_GPU_CHECK(x), where x returns/is of
// type 'cudaError_t'. This macro evaluates 'x' and raises an error if the
// result is not cudaSuccess.
#define ASC_GPU_CHECK(x)                                                 \
  do {                                                                   \
    cudaError_t asc_err_internal_var_name = (x);                         \
    if (asc_err_internal_var_name != cudaSuccess) {                      \
      ::asc::AscCudaError(asc_err_internal_var_name, #x, _ASC_FUNC_NAME, \
                          __FILE__, __LINE__);                           \
    }                                                                    \
  } while (0)
#else
#define ASC_DEVICE
#define ASC_HOST
#define ASC_LAMBDA
#define ASC_STREAM_SYNC
#endif  // ASC_USE_CUDA

// Define the ASC inner threading macros
#if defined(ASC_USE_CUDA) && defined(__CUDA_ARCH__)
#define ASC_SHARED __shared__
#define ASC_SYNC_THREAD __syncthreads()
#define ASC_BLOCK_ID(k) blockIdx.k
#define ASC_THREAD_ID(k) threadIdx.k
#define ASC_THREAD_SIZE(k) blockDim.k
#define ASC_FOREACH_THREAD(i, k, N) \
  for (int i = threadIdx.k; i < N; i += blockDim.k)
#define ASC_FOREACH_THREAD_DIRECT(i, k, N) if (const int i = threadIdx.k; i < N)
#endif

namespace asc {

#ifdef ASC_USE_CUDA
/// @brief Error handler for CUDA runtime calls (used by ASC_GPU_CHECK)
///
/// @param err CUDA error code
/// @param expr String representation of the CUDA call
/// @param func Function name where error occurred
/// @param file Source file name
/// @param line Line number
void AscCudaError(cudaError_t err, const char *expr, const char *func,
                  const char *file, int line);
#endif

/// @brief Allocate device memory
///
/// @param d_ptr Pointer to device pointer (output)
/// @param bytes Number of bytes to allocate
/// @return Device pointer (same as *d_ptr)
void *CuMemAlloc(void **d_ptr, size_t bytes);

/// @brief Allocate CUDA unified/managed memory
///
/// Managed memory is accessible from both host and device with automatic
/// migration handled by the CUDA runtime (requires CUDA 6.0+).
///
/// @param d_ptr Pointer to managed pointer (output)
/// @param bytes Number of bytes to allocate
/// @return Managed pointer (same as *d_ptr)
void *CuMallocManaged(void **d_ptr, size_t bytes);

/// @brief Allocate page-locked (pinned) host memory
///
/// Pinned memory provides faster host-device transfers and can be accessed
/// directly by the GPU via zero-copy.
///
/// @param ptr Pointer to host pointer (output)
/// @param bytes Number of bytes to allocate
/// @return Host pointer (same as *ptr)
void *CuMemAllocHostPinned(void **ptr, size_t bytes);

/// @brief Free device memory
///
/// @param d_ptr Device pointer to free
/// @return nullptr
void *CuMemFree(void *d_ptr);

/// @brief Free page-locked (pinned) host memory
///
/// @param ptr Host pointer to free
/// @return nullptr
void *CuMemFreeHostPinned(void *ptr);

/// @brief Copy memory from host to device (synchronous)
///
/// @param d_dst Destination device pointer
/// @param h_src Source host pointer
/// @param bytes Number of bytes to copy
/// @return Destination pointer (d_dst)
void *CuMemcpyHtoD(void *d_dst, const void *h_src, size_t bytes);

/// @brief Copy memory from host to device (asynchronous)
///
/// @param d_dst Destination device pointer
/// @param h_src Source host pointer
/// @param bytes Number of bytes to copy
/// @return Destination pointer (d_dst)
void *CuMemcpyHtoDAsync(void *d_dst, const void *h_src, size_t bytes);

/// @brief Copy memory from device to device (synchronous)
///
/// @param d_dst Destination device pointer
/// @param d_src Source device pointer
/// @param bytes Number of bytes to copy
/// @return Destination pointer (d_dst)
void *CuMemcpyDtoD(void *d_dst, const void *d_src, size_t bytes);

/// @brief Copy memory from device to device (asynchronous)
///
/// @param d_dst Destination device pointer
/// @param d_src Source device pointer
/// @param bytes Number of bytes to copy
/// @return Destination pointer (d_dst)
void *CuMemcpyDtoDAsync(void *d_dst, const void *d_src, size_t bytes);

/// @brief Copy memory from device to host (synchronous)
///
/// @param h_dst Destination host pointer
/// @param d_src Source device pointer
/// @param bytes Number of bytes to copy
/// @return Destination pointer (h_dst)
void *CuMemcpyDtoH(void *h_dst, const void *d_src, size_t bytes);

/// @brief Copy memory from device to host (asynchronous)
///
/// @param h_dst Destination host pointer
/// @param d_src Source device pointer
/// @param bytes Number of bytes to copy
/// @return Destination pointer (h_dst)
void *CuMemcpyDtoHAsync(void *h_dst, const void *d_src, size_t bytes);

/// @brief Check for CUDA errors from previous kernel launches
///
/// Calls cudaGetLastError() and aborts if an error occurred.
/// Use after kernel launches to detect execution errors.
void CuCheckLastError();

/// @brief Get the number of available CUDA devices
///
/// @return Number of CUDA-capable GPUs in the system
int CuGetDeviceCount();

/// @brief Check if a pointer points to device memory
///
/// Uses cudaPointerGetAttributes to determine if a pointer is a device pointer.
/// This is useful for implementing functions that need to handle both host and
/// device pointers differently.
///
/// @param ptr Pointer to check
/// @return true if ptr points to device memory, false otherwise
bool CuIsDevicePointer(const void* ptr);

}  // namespace asc

#endif  // ASC_CUDA_H_
