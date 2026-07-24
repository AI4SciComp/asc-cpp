// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/linalg/types.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_ALGEBRA_TYPES_H_
#define ASC_ALGEBRA_TYPES_H_

namespace asc {

/// @brief Matrix operation mode for BLAS/LAPACK-style routines.
enum class TransposeMode {
  kNoTranspose,  ///< Use A as stored.
  kTranspose,    ///< Use the transpose of A.
};

/// @brief Triangular storage selector.
enum class TriangleMode {
  kLower,  ///< Use the lower triangle.
  kUpper,  ///< Use the upper triangle.
};

/// @brief Diagonal interpretation for triangular matrices.
enum class DiagonalMode {
  kNonUnit,  ///< Diagonal entries are read from the matrix.
  kUnit,     ///< Diagonal entries are treated as one.
};

/// @brief Side selector for matrix-matrix operations.
enum class SideMode {
  kLeft,   ///< Apply the operator on the left.
  kRight,  ///< Apply the operator on the right.
};

}  // namespace asc

#endif  // ASC_ALGEBRA_TYPES_H_
