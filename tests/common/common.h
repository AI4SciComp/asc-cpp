// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/common/common.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_TESTS_COMMON_COMMON_H_
#define ASC_TESTS_COMMON_COMMON_H_

/// @file common.h
/// @brief Common test utilities and fixtures for all ASC tests
///
/// This header provides shared test infrastructure that works uniformly
/// across both CPU (host memory) and GPU (device memory) tests using
/// GoogleTest's typed test framework.

#include <gtest/gtest.h>
#include "asc/core/memory.h"

#ifdef ASC_USE_CUDA
#include "asc/core/cuda.h"
#endif

// ============================================================================
// Memory Type Tags for Typed Tests
// ============================================================================

using asc::MemoryType;

/// @brief Tag for CPU (host) memory tests
struct CPU {
  static constexpr MemoryType value = MemoryType::kHost;
  static constexpr const char* name = "CPU";
};

#ifdef ASC_USE_CUDA
/// @brief Tag for GPU (device) memory tests
struct GPU {
  static constexpr MemoryType value = MemoryType::kDevice;
  static constexpr const char* name = "GPU";
};

/// @brief Test both CPU and GPU when CUDA is enabled
using AllMemoryTypes = ::testing::Types<CPU, GPU>;
#else
/// @brief Test only CPU when CUDA is disabled
using AllMemoryTypes = ::testing::Types<CPU>;
#endif

namespace asc {

#ifdef ASC_USE_DOUBLE
inline constexpr double kRealTolerance = 1e-10;
inline constexpr double kLooseRealTolerance = 1e-8;
#else
inline constexpr float kRealTolerance = 1e-5f;
inline constexpr float kLooseRealTolerance = 1e-4f;
#endif

#ifdef ASC_USE_CUDA
inline bool CudaRuntimeIsAvailable() {
  static const bool available = [] {
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess || count <= 0) {
      cudaGetLastError();
      return false;
    }

    void* ptr = nullptr;
    err = cudaMalloc(&ptr, 1);
    if (err != cudaSuccess) {
      cudaGetLastError();
      return false;
    }
    err = cudaFree(ptr);
    if (err != cudaSuccess) {
      cudaGetLastError();
      return false;
    }
    return true;
  }();
  return available;
}
#endif

// ============================================================================
// Base Test Fixture Template
// ============================================================================

/// @brief Base test fixture for all multi-backend tests
/// @tparam MemType Memory type configuration (CPU or GPU)
///
/// This fixture provides helper methods to create arrays with the appropriate
/// memory type, allowing the same test code to run on both CPU and GPU.
template <typename MemType>
class BaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
#ifdef ASC_USE_CUDA
    if constexpr (MemType::value == MemoryType::kDevice) {
      if (!CudaRuntimeIsAvailable()) {
        GTEST_SKIP() << "CUDA runtime is unavailable for GPU typed tests";
      }
    }
#endif
  }

  /// @brief The memory type for this test instance
  static constexpr MemoryType kMemType = MemType::value;
};

#ifdef ASC_USE_DOUBLE
#define EXPECT_REAL_EQ(val1, val2) EXPECT_DOUBLE_EQ(val1, val2)
#else
#define EXPECT_REAL_EQ(val1, val2) EXPECT_FLOAT_EQ(val1, val2)
#endif

}  // namespace asc

#endif  // ASC_TESTS_COMMON_COMMON_H_
