// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/dsmarray.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

/// @file marray.cc
/// @brief Explicit template instantiations for DenseMArray
///
/// This file contains explicit instantiations of DenseMArray for common types
/// to reduce compilation time and binary size. By compiling these templates
/// once, we avoid recompiling them in every translation unit.

#include "asc/array/dsmarray.h"

namespace asc {

/// Macro for generating dynamic-size vector instantiations and aliases
#define ASC_MAKE_DYNAMIC_VECTOR(Type, Suffix) \
  template class DenseMArray<Type, DShape<1>>;

/// Macro for generating dynamic-size matrix instantiations and aliases
#define ASC_MAKE_DYNAMIC_MATRIX(Type, Suffix) \
  template class DenseMArray<Type, DShape<2>>;

/// Macro for generating static-size vector instantiations and aliases
#define ASC_MAKE_STATIC_VECTOR(Type, Suffix, Size) \
  template class DenseMArray<Type, MShape<Size>>;

/// Macro for generating static-size matrix instantiations and aliases
#define ASC_MAKE_STATIC_MATRIX(Type, Suffix, Size) \
  template class DenseMArray<Type, MShape<Size, Size>>;

// Generate instiantiations and aliases for dynamic vectors
ASC_MAKE_DYNAMIC_VECTOR(int, i)
ASC_MAKE_DYNAMIC_VECTOR(bool, b)
ASC_MAKE_DYNAMIC_VECTOR(real_t, r)

// Generate instiantiations and aliases for dynamic matrices
ASC_MAKE_DYNAMIC_MATRIX(int, i)
ASC_MAKE_DYNAMIC_MATRIX(bool, b)
ASC_MAKE_DYNAMIC_MATRIX(real_t, r)

// Generate instiantiations and aliases for static vectors
ASC_MAKE_STATIC_VECTOR(int, i, 2)
ASC_MAKE_STATIC_VECTOR(int, i, 3)
ASC_MAKE_STATIC_VECTOR(int, i, 4)
ASC_MAKE_STATIC_VECTOR(bool, b, 2)
ASC_MAKE_STATIC_VECTOR(bool, b, 3)
ASC_MAKE_STATIC_VECTOR(bool, b, 4)
ASC_MAKE_STATIC_VECTOR(real_t, r, 2)
ASC_MAKE_STATIC_VECTOR(real_t, r, 3)
ASC_MAKE_STATIC_VECTOR(real_t, r, 4)

// Generate instiantiations and aliases for static matrices
ASC_MAKE_STATIC_MATRIX(int, i, 2)
ASC_MAKE_STATIC_MATRIX(int, i, 3)
ASC_MAKE_STATIC_MATRIX(int, i, 4)
ASC_MAKE_STATIC_MATRIX(bool, b, 2)
ASC_MAKE_STATIC_MATRIX(bool, b, 3)
ASC_MAKE_STATIC_MATRIX(bool, b, 4)
ASC_MAKE_STATIC_MATRIX(real_t, r, 2)
ASC_MAKE_STATIC_MATRIX(real_t, r, 3)
ASC_MAKE_STATIC_MATRIX(real_t, r, 4)

#undef ASC_MAKE_DYNAMIC_VECTOR
#undef ASC_MAKE_DYNAMIC_MATRIX
#undef ASC_MAKE_STATIC_VECTOR
#undef ASC_MAKE_STATIC_MATRIX

}  // namespace asc
