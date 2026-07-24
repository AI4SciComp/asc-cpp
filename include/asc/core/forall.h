// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/forall.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_FORALL_H_
#define ASC_FORALL_H_

/// @file forall.h
/// @brief Portable parallel loop abstractions for heterogeneous computing
///
/// This module provides high-level parallel loop macros that automatically
/// dispatch to the appropriate backend (CPU, OpenMP, CUDA) based on runtime
/// configuration. It enables writing performance-portable code that runs
/// efficiently on both CPUs and GPUs with minimal code changes.
///
/// @par Key Macros:
/// - ASC_FORALL(i, N, ...): Standard 1D parallel loop
/// - ASC_FORALL_2D(i, N, X, Y, BZ, ...): 2D CUDA block configuration
/// - ASC_FORALL_3D(i, N, X, Y, Z, ...): 3D CUDA block configuration
/// - ASC_FORALL_SWITCH(use_dev, i, N, ...): Conditional device execution
/// - ASC_CUDA_FORALL(i, N, ...): Force CUDA backend (device-only)
///
/// @par Backend Selection:
/// The ForallWrap dispatcher selects backends in this priority order:
/// 1. CUDA (if enabled and use_dev=true)
/// 2. OpenMP (if enabled and use_dev=false)
/// 3. Sequential CPU (fallback)
///
/// @par Thread Safety:
/// Loop bodies should be thread-safe. Avoid shared mutable state.
///
/// @note Lambda captures should be by-value for CUDA compatibility

#include "asc/core/error.h"
#include "asc/core/memory.h"
#include "asc/core/device.h"

#ifdef ASC_USE_CUDA
#include "asc/core/cuda.h"
#endif

namespace asc {

// ASC pragma macros that can be used inside ASC_FORALL macros.
#define ASC_PRAGMA(X) _Pragma(#X)

// ASC_UNROLL pragma macro that can be used inside ASC_FORALL macros.
#if defined(ASC_USE_CUDA) && defined(__CUDA_ARCH__)
#ifdef __NVCC__
#define ASC_UNROLL(N) ASC_PRAGMA(unroll(N))
#else  // Assuming Clang CUDA
#define ASC_UNROLL(N) ASC_PRAGMA(unroll N)
#endif
#else
#define ASC_UNROLL(N)
#endif

#if defined(ASC_USE_CUDA)
#define ASC_CUDA_FORALL(i, N, ...) \
  CuWrap1d(N, [=] ASC_DEVICE(int i) { __VA_ARGS__ })
#else
#define ASC_CUDA_FORALL(i, N, ...) \
  do {                                \
    ASC_ABORT("CUDA forall requested but CUDA is not enabled."); \
  } while (false)
#endif

// The ASC_FORALL wrapper
#define ASC_FORALL(i, N, ...) \
  ForallWrap<1>(true, N, [=] ASC_HOST_DEVICE(int i) { __VA_ARGS__ })

// ASC_FORALL with a 2D CUDA block
#define ASC_FORALL_2D(i, N, X, Y, BZ, ...) \
  ForallWrap<2>(true, N, [=] ASC_HOST_DEVICE(int i) { __VA_ARGS__ }, X, Y, BZ)

// ASC_FORALL with a 3D CUDA block
#define ASC_FORALL_3D(i, N, X, Y, Z, ...) \
  ForallWrap<3>(true, N, [=] ASC_HOST_DEVICE(int i) { __VA_ARGS__ }, X, Y, Z)

// ASC_FORALL with a 3D CUDA block and grid
// With G=0, this is the same as ASC_FORALL_3D(i,N,X,Y,Z,...)
#define ASC_FORALL_3D_GRID(i, N, X, Y, Z, G, ...) \
  ForallWrap<3>(true, N, [=] ASC_HOST_DEVICE(int i) { __VA_ARGS__ }, X, Y, Z, G)

// ASC_FORALL that uses the basic CPU backend when use_dev is false.
#define ASC_FORALL_SWITCH(use_dev, i, N, ...) \
  ForallWrap<1>(use_dev, N, [=] ASC_HOST_DEVICE(int i) { __VA_ARGS__ })

// OpenMP backend
template <typename HBody>
void OmpWrap(const int N, HBody &&h_body) {
  ASC_VERIFY(N >= 0, "Forall loop size must be non-negative.");
#ifdef ASC_USE_OPENMP
#pragma omp parallel for
  for (int k = 0; k < N; k++) {
    h_body(k);
  }
#else
  ASC_CONTRACT_VAR(N);
  ASC_CONTRACT_VAR(h_body);
  ASC_ABORT("OpenMP requested for ASC but OpenMP is not enabled!");
#endif
}

// CUDA backend
// Only define CUDA kernels when compiling with nvcc
#if defined(ASC_USE_CUDA) && defined(__CUDACC__)

template <typename Body>
__global__ static void CuKernel1d(const int N, Body body) {
  const int k = blockDim.x * blockIdx.x + threadIdx.x;
  if (k >= N) {
    return;
  }
  body(k);
}

template <typename Body>
__global__ static void CuKernel2d(const int N, Body body) {
  const int k = blockIdx.x * blockDim.z + threadIdx.z;
  if (k >= N) {
    return;
  }
  body(k);
}

template <typename Body>
__global__ static void CuKernel3d(const int N, Body body) {
  for (int k = blockIdx.x; k < N; k += gridDim.x) {
    body(k);
  }
}

template <const int Blck = ASC_CUDA_BLOCKS, typename DBody>
void CuWrap1d(const int N, DBody &&d_body) {
  if (N == 0) {
    return;
  }
  const int kGrid = (N + Blck - 1) / Blck;
  CuKernel1d<<<kGrid, Blck>>>(N, d_body);
  ASC_GPU_CHECK(cudaGetLastError());
}

template <typename DBody>
void CuWrap2d(const int N, DBody &&d_body, const int X, const int Y,
              const int BZ) {
  if (N == 0) {
    return;
  }
  ASC_VERIFY(BZ > 0, "");
  const int kGrid = (N + BZ - 1) / BZ;
  const dim3 kBlck(X, Y, BZ);
  CuKernel2d<<<kGrid, kBlck>>>(N, d_body);
  ASC_GPU_CHECK(cudaGetLastError());
}

template <typename DBody>
void CuWrap3d(const int N, DBody &&d_body, const int X, const int Y,
              const int Z, const int G) {
  if (N == 0) {
    return;
  }
  const int kGrid = G == 0 ? N : G;
  const dim3 kBlck(X, Y, Z);
  CuKernel3d<<<kGrid, kBlck>>>(N, d_body);
  ASC_GPU_CHECK(cudaGetLastError());
}

template <int Dim>
struct CuWrap;

template <>
struct CuWrap<1> {
  template <const int Blck = ASC_CUDA_BLOCKS, typename DBody>
  static void run(const int N, DBody &&d_body, const int X, const int Y,
                  const int Z, const int G) {
    CuWrap1d<Blck>(N, d_body);
  }
};

template <>
struct CuWrap<2> {
  template <const int Blck = ASC_CUDA_BLOCKS, typename DBody>
  static void run(const int N, DBody &&d_body, const int X, const int Y,
                  const int Z, const int G) {
    CuWrap2d(N, d_body, X, Y, Z);
  }
};

template <>
struct CuWrap<3> {
  template <const int Blck = ASC_CUDA_BLOCKS, typename DBody>
  static void run(const int N, DBody &&d_body, const int X, const int Y,
                  const int Z, const int G) {
    CuWrap3d(N, d_body, X, Y, Z, G);
  }
};

#endif  // defined(ASC_USE_CUDA) && defined(__CUDACC__)

// The forall kernel body wrapper
template <const int Dim, typename DLambda, typename HLambda>
inline void ForallWrap(const bool use_dev, const int N, DLambda &&d_body,
                       HLambda &&h_body, const int X = 0, const int Y = 0,
                       const int Z = 0, const int G = 0) {
  ASC_VERIFY(N >= 0, "Forall loop size must be non-negative.");
  ASC_CONTRACT_VAR(X);
  ASC_CONTRACT_VAR(Y);
  ASC_CONTRACT_VAR(Z);
  ASC_CONTRACT_VAR(G);
  ASC_CONTRACT_VAR(d_body);
  if (!use_dev) {
    goto backend_cpu;
  }

#if defined(ASC_USE_CUDA) && defined(__CUDACC__)
  if (Device::Allows(Backend::kCuda)) {
    return CuWrap<Dim>::run(N, d_body, X, Y, Z, G);
  }
#elif defined(ASC_USE_CUDA)
  if (Device::Allows(Backend::kCuda)) {
    ASC_ABORT("CUDA backend requested from a translation unit that was not "
                 "compiled by the CUDA compiler.");
  }
#endif

#ifdef ASC_USE_OPENMP
  if (Device::Allows(Backend::kOmp)) {
    return OmpWrap(N, h_body);
  }
#endif

backend_cpu:
  for (int k = 0; k < N; k++) {
    h_body(k);
  }
}

template <const int Dim, typename Lambda>
inline void ForallWrap(const bool use_dev, const int N, Lambda &&body,
                       const int X = 0, const int Y = 0, const int Z = 0,
                       const int G = 0) {
  ForallWrap<Dim>(use_dev, N, body, body, X, Y, Z, G);
}

template <typename Lambda>
inline void Forall(int N, Lambda &&body) {
  ForallWrap<1>(true, N, body);
}

template <typename Lambda>
inline void ForallSwitch(bool use_dev, int N, Lambda &&body) {
  ForallWrap<1>(use_dev, N, body);
}

template <typename Lambda>
inline void Forall2d(int N, int X, int Y, Lambda &&body) {
  ForallWrap<2>(true, N, body, X, Y, 1);
}

template <typename Lambda>
inline void Forall2dBatch(int N, int X, int Y, int BZ, Lambda &&body) {
  ForallWrap<2>(true, N, body, X, Y, BZ);
}

template <typename Lambda>
inline void Forall3d(int N, int X, int Y, int Z, Lambda &&body) {
  ForallWrap<3>(true, N, body, X, Y, Z, 0);
}

template <typename Lambda>
inline void Forall3dGrid(int N, int X, int Y, int Z, int G, Lambda &&body) {
  ForallWrap<3>(true, N, body, X, Y, Z, G);
}

}  // namespace asc

#endif  // ASC_FORALL_H_
