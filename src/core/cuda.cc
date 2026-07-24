// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/cuda.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include "asc/core/globals.h"
#include "asc/core/cuda.h"

namespace asc {

#ifdef ASC_USE_CUDA
void AscCudaError(cudaError_t err, const char* expr, const char* func,
                  const char* file, int line) {
  asc::merr << "\n\nCUDA error: (" << expr << ") failed with error:\n --> "
            << cudaGetErrorString(err) << "\n ... in function: " << func
            << "\n ... in file: " << file << ':' << line << '\n';
  asc::Error();
}
#endif

void* CuMemAlloc(void** dptr, size_t bytes) {
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMalloc(dptr, bytes));
#endif
  return *dptr;
}

void* CuMallocManaged(void** dptr, size_t bytes) {
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMallocManaged(dptr, bytes));
#endif
  return *dptr;
}

void* CuMemAllocHostPinned(void** ptr, size_t bytes) {
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMallocHost(ptr, bytes));
#endif
  return *ptr;
}

void* CuMemFree(void* dptr) {
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaFree(dptr));
#endif
  return dptr;
}

void* CuMemFreeHostPinned(void* ptr) {
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaFreeHost(ptr));
#endif
  return ptr;
}

void* CuMemcpyHtoD(void* dst, const void* src, size_t bytes) {
  static_cast<void>(src);
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMemcpy(dst, src, bytes, cudaMemcpyHostToDevice));
#endif
  return dst;
}

void* CuMemcpyHtoDAsync(void* dst, const void* src, size_t bytes) {
  static_cast<void>(src);
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMemcpyAsync(dst, src, bytes, cudaMemcpyHostToDevice));
#endif
  return dst;
}

void* CuMemcpyDtoD(void* dst, const void* src, size_t bytes) {
  static_cast<void>(src);
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMemcpy(dst, src, bytes, cudaMemcpyDeviceToDevice));
#endif
  return dst;
}

void* CuMemcpyDtoDAsync(void* dst, const void* src, size_t bytes) {
  static_cast<void>(src);
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMemcpyAsync(dst, src, bytes, cudaMemcpyDeviceToDevice));
#endif
  return dst;
}

void* CuMemcpyDtoH(void* dst, const void* src, size_t bytes) {
  static_cast<void>(src);
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMemcpy(dst, src, bytes, cudaMemcpyDeviceToHost));
#endif
  return dst;
}

void* CuMemcpyDtoHAsync(void* dst, const void* src, size_t bytes) {
  static_cast<void>(src);
  static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaMemcpyAsync(dst, src, bytes, cudaMemcpyDeviceToHost));
#endif
  return dst;
}

void CuCheckLastError() {
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaGetLastError());
#endif
}

int CuGetDeviceCount() {
  int num_gpus = -1;
#ifdef ASC_USE_CUDA
  ASC_GPU_CHECK(cudaGetDeviceCount(&num_gpus));
#endif
  return num_gpus;
}

bool CuIsDevicePointer(const void* ptr) {
  static_cast<void>(ptr);
#ifdef ASC_USE_CUDA
  if (ptr == nullptr) return false;

  cudaPointerAttributes attr;
  cudaError_t err = cudaPointerGetAttributes(&attr, ptr);

  // If the query fails, assume it's a host pointer
  if (err != cudaSuccess) {
    cudaGetLastError();  // Clear the error
    return false;
  }

  // Check if the pointer is a device pointer
  return (attr.type == cudaMemoryTypeDevice);
#else
  return false;
#endif
}

}  // namespace asc
