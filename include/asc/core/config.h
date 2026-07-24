// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/core/config.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_CORE_CONFIG_H_
#define ASC_CORE_CONFIG_H_

/// @file config.h
/// @brief Stable public configuration facade for asc-cpp.
///
/// CMake writes build-specific feature switches to an internal generated
/// header. This facade owns the public types and portability macros so the
/// installed API does not consist of a CMake template.

#ifdef ASC_CONFIG_FILE
#include ASC_CONFIG_FILE
#else
#include <asc/config/_config.h>
#endif

#if defined(ASC_USE_CUDA) && defined(__CUDACC__)
#define ASC_HOST_DEVICE __host__ __device__
#else
#define ASC_HOST_DEVICE
#endif

#if defined(ASC_USE_SINGLE) && defined(ASC_USE_DOUBLE)
#error "ASC single and double precision cannot both be enabled"
#endif

namespace asc {

#if defined(ASC_USE_SINGLE)
using real_t = float;
#elif defined(ASC_USE_DOUBLE)
using real_t = double;
#else
#error "ASC requires either single or double precision"
#endif

ASC_HOST_DEVICE constexpr real_t operator""_r(long double value) {
  return static_cast<real_t>(value);
}

ASC_HOST_DEVICE constexpr real_t operator""_r(  // NOLINT(runtime/int)
    unsigned long long value) {                 // NOLINT(runtime/int)
  return static_cast<real_t>(value);
}

}  // namespace asc

#if defined(__GNUC__) || defined(__clang__)
#define ASC_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define ASC_DEPRECATED __declspec(deprecated)
#else
#define ASC_DEPRECATED
#endif

#if defined(_WIN32) && !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif

#if defined(_WIN32) && defined(ASC_SHARED_BUILD)
#ifdef ASC_BUILDING_LIBRARY
#define ASC_EXPORT __declspec(dllexport)
#else
#define ASC_EXPORT __declspec(dllimport)
#endif
#else
#define ASC_EXPORT
#endif

#define ASC_STACK_ALLOCATION_LIMIT 131072

#endif  // ASC_CORE_CONFIG_H_
