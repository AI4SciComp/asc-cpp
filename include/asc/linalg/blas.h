// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/linalg/blas.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_BLAS_H_
#define ASC_BLAS_H_

/// @file blas.h
/// @brief NumPy-style tensor operations for arbitrary-dimensional arrays
///
/// This module provides comprehensive tensor operations following NumPy's
/// design philosophy, supporting both DenseMArray and SparseMArray with
/// arbitrary dimensions.
///
/// @par Operation Categories:
/// 1. **Basic Math Operations**: Abs, Sqrt, Square, Cbrt, Reciprocal, Sign,
/// Negative
/// 2. **Exponential & Logarithmic**: Exp, Exp2, Expm1, Log, Log2, Log10,
/// Log1p
/// 3. **Trigonometric Functions**: Sin, Cos, Tan, Asin, Acos, Atan, Atan2
/// 4. **Hyperbolic Functions**: Sinh, Cosh, Tanh
/// 5. **Rounding Functions**: Floor, Ceil, Round, Trunc
/// 6. **Special Functions**: Erf, Erfc, Tgamma, Lgamma, Fabs
/// 7. **Binary Arithmetic**: Max, Min, Pow, Mod, Remainder, Hypot, Clip
/// 8. **Logical Operations**: LogicalNot, LogicalAnd, LogicalOr, LogicalXor
/// 9. **Comparison Operations**: Greater, GreaterEqual, Less, LessEqual,
/// Equal, NotEqual
/// 10. **Testing Functions**: IsNan, IsInf, IsFinite
/// 11. **Linear Algebra**: Dot, MatMul, Outer
/// 12. **Tensor Contractions**: TensorDot, Einsum, Kron, Cross
///
/// @par Design Principles:
/// - All operations work with arbitrary-dimensional tensors where applicable
/// - Support both CPU and GPU execution via ASC_FORALL_SWITCH
/// - Consistent API across dense and sparse arrays
/// - Efficient in-place operations
/// - Reduction operations (Sum, Mean, Norm, etc.) are member functions in
/// DenseMArray/SparseMArray
///
/// @par Usage Examples:
/// @code
/// using namespace asc;
///
/// // Element-wise operations
/// VectorXr x(100), y(100);
/// x = 2.0;
/// Sqrt(x, y);  // y = sqrt(x)
///
/// // Matrix multiplication
/// MatrixXr A(3, 4), B(4, 5), C(3, 5);
/// MatMul(A, B, C);  // C = A @ B
///
/// // Dot product
/// VectorXr a(10), b(10);
/// real_t result = Dot(a, b);
///
/// // Tensor contraction
/// MatrixXr X(10, 20), Y(20, 30), Z(10, 30);
/// UArray<int> axes_x({1}), axes_y({0});
/// TensorDot(X, Y, axes_x, axes_y, Z);  // Contract axis 1 of X with axis 0
/// of Y
/// @endcode

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

#include "asc/linalg/types.h"
#include "asc/array/marray.h"
#include "asc/core/math.h"
#include "asc/core/forall.h"

namespace asc {

namespace internal {

template <typename Array>
inline constexpr bool kResizableDenseArray =
    DenseMArrayLike<Array> && Array::ShapeType::IsDynamic() &&
    !std::is_same_v<typename Array::LayoutType, LayoutStride>;

template <DenseTensorLike Vector>
inline void VerifyBlasVectorSize(const Vector& x, int size,
                                 const char* name) {
  if constexpr (Vector::GetRank() != 1) {
    ASC_VERIFY(false, name << " must be a vector");
  } else {
    ASC_VERIFY(x.GetSize() == size,
                  name << " has size " << x.GetSize() << ", expected "
                       << size);
  }
}

template <DenseTensorLike Vector>
inline void EnsureBlasVectorSize(Vector& x, int size, const char* name) {
  if constexpr (Vector::GetRank() != 1) {
    ASC_VERIFY(false, name << " must be a vector");
  } else if (x.GetSize() != size) {
    if constexpr (kResizableDenseArray<Vector>) {
      x.SetShape(DShape<1>(size));
    } else {
      ASC_VERIFY(false,
                    name << " has size " << x.GetSize() << ", expected "
                         << size);
    }
  }
}

template <DenseTensorLike Matrix>
inline void VerifyBlasMatrixSize(const Matrix& A, int rows, int cols,
                                 const char* name) {
  if constexpr (Matrix::GetRank() != 2) {
    ASC_VERIFY(false, name << " must be a matrix");
  } else {
    ASC_VERIFY(A.GetExtent(0) == rows && A.GetExtent(1) == cols,
                  name << " has shape (" << A.GetExtent(0) << ", "
                       << A.GetExtent(1) << "), expected (" << rows << ", "
                       << cols << ")");
  }
}

template <DenseTensorLike Matrix>
inline void EnsureBlasMatrixSize(Matrix& A, int rows, int cols,
                                 const char* name) {
  if constexpr (Matrix::GetRank() != 2) {
    ASC_VERIFY(false, name << " must be a matrix");
  } else if (A.GetExtent(0) != rows || A.GetExtent(1) != cols) {
    if constexpr (kResizableDenseArray<Matrix>) {
      A.SetShape(DShape<2>(rows, cols));
    } else {
      ASC_VERIFY(false,
                    name << " has shape (" << A.GetExtent(0) << ", "
                         << A.GetExtent(1) << "), expected (" << rows << ", "
                         << cols << ")");
    }
  }
}

template <typename T, typename Map>
inline T TriangularValue(const T* data, const Map& map, int row, int col,
                         TriangleMode uplo, DiagonalMode diag) {
  if (row == col && diag == DiagonalMode::kUnit) return T(1);
  if (uplo == TriangleMode::kLower) {
    return row >= col ? data[map(row, col)] : T(0);
  }
  return row <= col ? data[map(row, col)] : T(0);
}

template <typename T, typename Map>
inline T TriangularOpValue(const T* data, const Map& map, int row, int col,
                           TriangleMode uplo, TransposeMode trans,
                           DiagonalMode diag) {
  return trans == TransposeMode::kNoTranspose
             ? TriangularValue(data, map, row, col, uplo, diag)
             : TriangularValue(data, map, col, row, uplo, diag);
}

inline TransposeMode ToggleTranspose(TransposeMode trans) {
  return trans == TransposeMode::kNoTranspose ? TransposeMode::kTranspose
                                              : TransposeMode::kNoTranspose;
}

}  // namespace internal

// ============================================================================
// Category 1: Basic Math Operations
// ============================================================================

/// @brief Element-wise absolute value
template <TensorLike Tensor>
inline void Abs(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::abs(xdata[i]););
}

/// @brief Element-wise square root
template <TensorLike Tensor>
inline void Sqrt(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::sqrt(xdata[i]););
}

/// @brief Element-wise square function
template <TensorLike Tensor>
inline void Square(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, {
    const T val = xdata[i];
    ydata[i] = val * val;
  });
}

/// @brief Element-wise cube root function
template <TensorLike Tensor>
inline void Cbrt(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::cbrt(xdata[i]););
}

/// @brief Element-wise reciprocal function (1/x)
template <TensorLike Tensor>
inline void Reciprocal(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = T(1) / xdata[i];);
}

/// @brief Element-wise sign function (-1, 0, or 1)
template <TensorLike Tensor>
inline void Sign(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, {
    const T val = xdata[i];
    ydata[i] = (val > T(0)) ? T(1) : ((val < T(0)) ? T(-1) : T(0));
  });
}

/// @brief Element-wise negative (unary minus)
template <TensorLike Tensor>
inline void Negative(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = -xdata[i];);
}

/// @brief Element-wise absolute value (floating-point version)
template <TensorLike Tensor>
inline void Fabs(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::fabs(xdata[i]););
}

// ============================================================================
// Category 2: Exponential & Logarithmic Functions
// ============================================================================

/// @brief Element-wise exponential function
template <TensorLike Tensor>
inline void Exp(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::exp(xdata[i]););
}

/// @brief Element-wise base-2 exponential (2^x)
template <TensorLike Tensor>
inline void Exp2(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::exp2(xdata[i]););
}

/// @brief Element-wise exp(x) - 1 (accurate for small x)
template <TensorLike Tensor>
inline void Expm1(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::expm1(xdata[i]););
}

/// @brief Element-wise natural logarithm
template <TensorLike Tensor>
inline void Log(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::log(xdata[i]););
}

/// @brief Element-wise base-2 logarithm
template <TensorLike Tensor>
inline void Log2(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::log2(xdata[i]););
}

/// @brief Element-wise base-10 logarithm
template <TensorLike Tensor>
inline void Log10(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::log10(xdata[i]););
}

/// @brief Element-wise log(1 + x) (accurate for small x)
template <TensorLike Tensor>
inline void Log1p(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::log1p(xdata[i]););
}

// ============================================================================
// Category 3: Trigonometric Functions
// ============================================================================

/// @brief Element-wise sine function
template <TensorLike Tensor>
inline void Sin(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::sin(xdata[i]););
}

/// @brief Element-wise cosine function
template <TensorLike Tensor>
inline void Cos(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::cos(xdata[i]););
}

/// @brief Element-wise tangent function
template <TensorLike Tensor>
inline void Tan(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::tan(xdata[i]););
}

/// @brief Element-wise arc sine function
template <TensorLike Tensor>
inline void Asin(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::asin(xdata[i]););
}

/// @brief Element-wise arc cosine function
template <TensorLike Tensor>
inline void Acos(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::acos(xdata[i]););
}

/// @brief Element-wise arc tangent function
template <TensorLike Tensor>
inline void Atan(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::atan(xdata[i]););
}

/// @brief Element-wise arctangent of y/x (with correct quadrant)
template <TensorLike Tensor>
inline void Atan2(const Tensor& y, const Tensor& x, Tensor& z) {
  ASC_ASSERT(y.GetSize() == x.GetSize() && y.GetSize() == z.GetSize(),
                "tensor size mismatch");
  const int N = y.GetSize();
  using T = typename Tensor::ValueType;
  const T* ydata = y.Read();
  const T* xdata = x.Read();
  T* zdata = z.ReadWrite();
  const bool use_dev = y.UseDevice();
  z.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, zdata[i] = std::atan2(ydata[i], xdata[i]););
}

// ============================================================================
// Category 4: Hyperbolic Functions
// ============================================================================

/// @brief Element-wise hyperbolic sine function
template <TensorLike Tensor>
inline void Sinh(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::sinh(xdata[i]););
}

/// @brief Element-wise hyperbolic cosine function
template <TensorLike Tensor>
inline void Cosh(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::cosh(xdata[i]););
}

/// @brief Element-wise hyperbolic tangent function
template <TensorLike Tensor>
inline void Tanh(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::tanh(xdata[i]););
}

// ============================================================================
// Category 5: Rounding Functions
// ============================================================================

/// @brief Element-wise floor function
template <TensorLike Tensor>
inline void Floor(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::floor(xdata[i]););
}

/// @brief Element-wise ceiling function
template <TensorLike Tensor>
inline void Ceil(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::ceil(xdata[i]););
}

/// @brief Element-wise rounding to nearest integer
template <TensorLike Tensor>
inline void Round(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::round(xdata[i]););
}

/// @brief Element-wise truncation to integer
template <TensorLike Tensor>
inline void Trunc(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::trunc(xdata[i]););
}

// ============================================================================
// Category 6: Special Functions
// ============================================================================

/// @brief Element-wise error function
template <TensorLike Tensor>
inline void Erf(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::erf(xdata[i]););
}

/// @brief Element-wise complementary error function
template <TensorLike Tensor>
inline void Erfc(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::erfc(xdata[i]););
}

/// @brief Element-wise gamma function
template <TensorLike Tensor>
inline void Tgamma(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::tgamma(xdata[i]););
}

/// @brief Element-wise log-gamma function
template <TensorLike Tensor>
inline void Lgamma(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = std::lgamma(xdata[i]););
}

// ============================================================================
// Category 7: Binary Arithmetic Operations
// ============================================================================

/// @brief Element-wise maximum of two tensors
template <TensorLike Tensor>
inline void Max(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] > bdata[i]) ? adata[i] : bdata[i];);
}

/// @brief Element-wise minimum of two tensors
template <TensorLike Tensor>
inline void Min(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] < bdata[i]) ? adata[i] : bdata[i];);
}

/// @brief Element-wise power function
template <TensorLike Tensor>
inline void Pow(const Tensor& base, const Tensor& exp, Tensor& result) {
  ASC_ASSERT(base.GetSize() == exp.GetSize() &&
                    base.GetSize() == result.GetSize(),
                "tensor size mismatch");
  const int N = base.GetSize();
  using T = typename Tensor::ValueType;
  const T* base_data = base.Read();
  const T* exp_data = exp.Read();
  T* result_data = result.ReadWrite();
  const bool use_dev = base.UseDevice();
  result.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       result_data[i] = std::pow(base_data[i], exp_data[i]););
}

/// @brief Element-wise power with scalar exponent
template <TensorLike Tensor, Arithmetic Scalar>
inline void Pow(const Tensor& base, const Scalar& exp, Tensor& result) {
  ASC_ASSERT(base.GetSize() == result.GetSize(), "tensor size mismatch");
  const int N = base.GetSize();
  using T = typename Tensor::ValueType;
  const T* base_data = base.Read();
  T* result_data = result.ReadWrite();
  const T exponent = static_cast<T>(exp);
  const bool use_dev = base.UseDevice();
  result.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       result_data[i] = std::pow(base_data[i], exponent););
}

/// @brief Element-wise modulo operation
template <TensorLike Tensor>
inline void Mod(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, cdata[i] = std::fmod(adata[i], bdata[i]););
}

/// @brief Element-wise remainder operation
template <TensorLike Tensor>
inline void Remainder(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, cdata[i] = std::remainder(adata[i], bdata[i]););
}

/// @brief Element-wise hypot: sqrt(x^2 + y^2)
template <TensorLike Tensor>
inline void Hypot(const Tensor& x, const Tensor& y, Tensor& z) {
  ASC_ASSERT(x.GetSize() == y.GetSize() && x.GetSize() == z.GetSize(),
                "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  const T* ydata = y.Read();
  T* zdata = z.ReadWrite();
  const bool use_dev = x.UseDevice();
  z.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, zdata[i] = std::hypot(xdata[i], ydata[i]););
}

/// @brief Clip (limit) tensor values to a range [min_val, max_val]
template <TensorLike Tensor, Arithmetic Scalar>
inline void Clip(const Tensor& x, const Scalar& min_val, const Scalar& max_val,
                 Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  ASC_ASSERT(min_val <= max_val, "min_val must be <= max_val");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const T lower = static_cast<T>(min_val);
  const T upper = static_cast<T>(max_val);
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, {
    const T val = xdata[i];
    ydata[i] = (val < lower) ? lower : ((val > upper) ? upper : val);
  });
}

// Note: Element-wise addition, subtraction, multiplication, and division
// are available via operator overloads (+, -, *, /) in the MObject base class.

// ============================================================================
// Category 8: Logical Operations
// ============================================================================

/// @brief Element-wise logical NOT
template <TensorLike Tensor>
inline void LogicalNot(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N, ydata[i] = !xdata[i] ? T(1) : T(0););
}

/// @brief Element-wise logical AND
template <TensorLike Tensor>
inline void LogicalAnd(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] && bdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise logical OR
template <TensorLike Tensor>
inline void LogicalOr(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] || bdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise logical XOR
template <TensorLike Tensor>
inline void LogicalXor(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (static_cast<bool>(adata[i]) !=
                                   static_cast<bool>(bdata[i]))
                                      ? T(1)
                                      : T(0););
}

// ============================================================================
// Category 9: Comparison Operations
// ============================================================================

/// @brief Element-wise comparison: greater than
template <TensorLike Tensor>
inline void Greater(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] > bdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise comparison: greater than or equal
template <TensorLike Tensor>
inline void GreaterEqual(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] >= bdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise comparison: less than
template <TensorLike Tensor>
inline void Less(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] < bdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise comparison: less than or equal
template <TensorLike Tensor>
inline void LessEqual(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] <= bdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise comparison: equal
template <TensorLike Tensor>
inline void Equal(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] == bdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise comparison: not equal
template <TensorLike Tensor>
inline void NotEqual(const Tensor& a, const Tensor& b, Tensor& c) {
  ASC_ASSERT(a.GetSize() == b.GetSize() && a.GetSize() == c.GetSize(),
                "tensor size mismatch");
  const int N = a.GetSize();
  using T = typename Tensor::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       cdata[i] = (adata[i] != bdata[i]) ? T(1) : T(0););
}

// ============================================================================
// Category 10: Testing Functions
// ============================================================================

/// @brief Element-wise test for NaN
template <TensorLike Tensor>
inline void IsNan(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       ydata[i] = std::isnan(xdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise test for infinity
template <TensorLike Tensor>
inline void IsInf(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       ydata[i] = std::isinf(xdata[i]) ? T(1) : T(0););
}

/// @brief Element-wise test for finite
template <TensorLike Tensor>
inline void IsFinite(const Tensor& x, Tensor& y) {
  ASC_ASSERT(x.GetSize() == y.GetSize(), "tensor size mismatch");
  const int N = x.GetSize();
  using T = typename Tensor::ValueType;
  const T* xdata = x.Read();
  T* ydata = y.ReadWrite();
  const bool use_dev = x.UseDevice();
  y.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, N,
                       ydata[i] = std::isfinite(xdata[i]) ? T(1) : T(0););
}

// ============================================================================
// Category 11: Linear Algebra Operations
// ============================================================================

/// @brief Copy a dense vector: y = x.
template <DenseTensorLike VectorX, DenseTensorLike VectorY>
inline void Copy(const VectorX& x, VectorY& y) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");
  internal::EnsureBlasVectorSize(y, x.GetSize(), "y");

  using YT = typename VectorY::ElementType;
  const auto& x_map = x.GetMap();
  const auto& y_map = y.GetMap();
  const auto* x_data = x.HostRead();
  auto* y_data = y.HostWrite();
  for (int i = 0; i < x.GetSize(); ++i) {
    y_data[y_map(i)] = static_cast<YT>(x_data[x_map(i)]);
  }
}

/// @brief Swap two dense vectors in place.
template <DenseTensorLike VectorX, DenseTensorLike VectorY>
inline void Swap(VectorX& x, VectorY& y) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");
  internal::VerifyBlasVectorSize(y, x.GetSize(), "y");

  using XT = typename VectorX::ElementType;
  using YT = typename VectorY::ElementType;
  const auto& x_map = x.GetMap();
  const auto& y_map = y.GetMap();
  auto* x_data = x.HostReadWrite();
  auto* y_data = y.HostReadWrite();
  for (int i = 0; i < x.GetSize(); ++i) {
    const XT tmp = x_data[x_map(i)];
    x_data[x_map(i)] = static_cast<XT>(y_data[y_map(i)]);
    y_data[y_map(i)] = static_cast<YT>(tmp);
  }
}

/// @brief Scale a dense vector in place: x = alpha * x.
template <DenseTensorLike Vector>
inline void Scal(typename Vector::ValueType alpha, Vector& x) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");

  using T = typename Vector::ElementType;
  const auto& x_map = x.GetMap();
  auto* x_data = x.HostReadWrite();
  for (int i = 0; i < x.GetSize(); ++i) {
    x_data[x_map(i)] = static_cast<T>(alpha * x_data[x_map(i)]);
  }
}

/// @brief Add a scaled vector: y = alpha * x + y.
template <DenseTensorLike VectorX, DenseTensorLike VectorY>
inline void Axpy(typename VectorY::ValueType alpha, const VectorX& x,
                 VectorY& y) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");
  internal::EnsureBlasVectorSize(y, x.GetSize(), "y");

  using YT = typename VectorY::ElementType;
  const auto& x_map = x.GetMap();
  const auto& y_map = y.GetMap();
  const auto* x_data = x.HostRead();
  auto* y_data = y.HostReadWrite();
  for (int i = 0; i < x.GetSize(); ++i) {
    y_data[y_map(i)] =
        static_cast<YT>(alpha * x_data[x_map(i)] + y_data[y_map(i)]);
  }
}

/// @brief Affine vector update: y = alpha * x + beta * y.
template <DenseTensorLike VectorX, DenseTensorLike VectorY>
inline void Axpby(typename VectorY::ValueType alpha, const VectorX& x,
                  typename VectorY::ValueType beta, VectorY& y) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");
  internal::EnsureBlasVectorSize(y, x.GetSize(), "y");

  using YT = typename VectorY::ElementType;
  const auto& x_map = x.GetMap();
  const auto& y_map = y.GetMap();
  const auto* x_data = x.HostRead();
  auto* y_data = y.HostReadWrite();
  for (int i = 0; i < x.GetSize(); ++i) {
    y_data[y_map(i)] = static_cast<YT>(alpha * x_data[x_map(i)] +
                                       beta * y_data[y_map(i)]);
  }
}

/// @brief Euclidean vector norm.
template <DenseTensorLike Vector>
inline typename Vector::ValueType Nrm2(const Vector& x) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");

  using T = typename Vector::ValueType;
  const auto& x_map = x.GetMap();
  const auto* x_data = x.HostRead();
  T sum = T(0);
  for (int i = 0; i < x.GetSize(); ++i) {
    const T value = x_data[x_map(i)];
    sum += value * value;
  }
  return std::sqrt(sum);
}

/// @brief Sum of absolute values of a dense vector.
template <DenseTensorLike Vector>
inline typename Vector::ValueType Asum(const Vector& x) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");

  using T = typename Vector::ValueType;
  const auto& x_map = x.GetMap();
  const auto* x_data = x.HostRead();
  T sum = T(0);
  for (int i = 0; i < x.GetSize(); ++i) {
    sum += std::abs(x_data[x_map(i)]);
  }
  return sum;
}

/// @brief Index of the entry with maximum absolute value.
template <DenseTensorLike Vector>
inline int Iamax(const Vector& x) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");
  if (x.GetSize() == 0) return -1;

  const auto& x_map = x.GetMap();
  const auto* x_data = x.HostRead();
  int index = 0;
  auto best = std::abs(x_data[x_map(0)]);
  for (int i = 1; i < x.GetSize(); ++i) {
    const auto value = std::abs(x_data[x_map(i)]);
    if (value > best) {
      best = value;
      index = i;
    }
  }
  return index;
}

/// @brief Inner product / dot product (free function wrapper)
/// @param a First tensor (flattened for computation)
/// @param b Second tensor (flattened for computation)
/// @return Scalar result of inner product
///
/// @note For 1D tensors: standard dot product
/// @note For higher-D tensors: flattens both and computes dot product
/// @note Can also be called as a member function: a.Dot(b)
template <TensorLike Tensor, typename T = typename Tensor::ValueType>
inline T Dot(const Tensor& a, const Tensor& b) {
  return a.Dot(b);
}

/// @brief General matrix-vector multiply: y = alpha * op(A) * x + beta * y.
template <DenseTensorLike Matrix, DenseTensorLike VectorX,
          DenseTensorLike VectorY>
inline void Gemv(
    const Matrix& A, const VectorX& x, VectorY& y,
    typename Matrix::ValueType alpha = typename Matrix::ValueType(1),
    typename Matrix::ValueType beta = typename Matrix::ValueType(0),
    TransposeMode trans = TransposeMode::kNoTranspose) {
  if constexpr (Matrix::GetRank() != 2) {
    ASC_VERIFY(false, "A must be a matrix");
  } else {
    const int rows = A.GetExtent(0);
    const int cols = A.GetExtent(1);
    const int y_size = trans == TransposeMode::kNoTranspose ? rows : cols;
    const int x_size = trans == TransposeMode::kNoTranspose ? cols : rows;

    internal::VerifyBlasVectorSize(x, x_size, "x");
    internal::EnsureBlasVectorSize(y, y_size, "y");

    using YT = typename VectorY::ElementType;
    const auto& a_map = A.GetMap();
    const auto& x_map = x.GetMap();
    const auto& y_map = y.GetMap();
    const auto* a_data = A.HostRead();
    const auto* x_data = x.HostRead();
    auto* y_data = y.HostReadWrite();

    for (int i = 0; i < y_size; ++i) {
      typename Matrix::ValueType sum = typename Matrix::ValueType(0);
      for (int j = 0; j < x_size; ++j) {
        const auto a_value =
            trans == TransposeMode::kNoTranspose ? a_data[a_map(i, j)]
                                                 : a_data[a_map(j, i)];
        sum += a_value * x_data[x_map(j)];
      }
      const auto old_y =
          beta == typename Matrix::ValueType(0) ? typename Matrix::ValueType(0)
                                                : y_data[y_map(i)];
      y_data[y_map(i)] = static_cast<YT>(alpha * sum + beta * old_y);
    }
  }
}

/// @brief Symmetric matrix-vector multiply.
template <DenseTensorLike Matrix, DenseTensorLike VectorX,
          DenseTensorLike VectorY>
inline void Symv(
    const Matrix& A, const VectorX& x, VectorY& y,
    typename Matrix::ValueType alpha = typename Matrix::ValueType(1),
    typename Matrix::ValueType beta = typename Matrix::ValueType(0),
    TriangleMode uplo = TriangleMode::kLower) {
  if constexpr (Matrix::GetRank() != 2) {
    ASC_VERIFY(false, "A must be a matrix");
  } else {
    const int n = A.GetExtent(0);
    internal::VerifyBlasMatrixSize(A, n, n, "A");
    internal::VerifyBlasVectorSize(x, n, "x");
    internal::EnsureBlasVectorSize(y, n, "y");

    using YT = typename VectorY::ElementType;
    const auto& a_map = A.GetMap();
    const auto& x_map = x.GetMap();
    const auto& y_map = y.GetMap();
    const auto* a_data = A.HostRead();
    const auto* x_data = x.HostRead();
    auto* y_data = y.HostReadWrite();

    for (int i = 0; i < n; ++i) {
      typename Matrix::ValueType sum = typename Matrix::ValueType(0);
      for (int j = 0; j < n; ++j) {
        const auto a_value =
            (uplo == TriangleMode::kLower)
                ? (i >= j ? a_data[a_map(i, j)] : a_data[a_map(j, i)])
                : (i <= j ? a_data[a_map(i, j)] : a_data[a_map(j, i)]);
        sum += a_value * x_data[x_map(j)];
      }
      const auto old_y =
          beta == typename Matrix::ValueType(0) ? typename Matrix::ValueType(0)
                                                : y_data[y_map(i)];
      y_data[y_map(i)] = static_cast<YT>(alpha * sum + beta * old_y);
    }
  }
}

/// @brief Rank-1 update: A = alpha * x * y^T + A.
template <DenseTensorLike VectorX, DenseTensorLike VectorY,
          DenseTensorLike Matrix>
inline void Ger(typename Matrix::ValueType alpha, const VectorX& x,
                const VectorY& y, Matrix& A) {
  internal::VerifyBlasVectorSize(x, x.GetSize(), "x");
  internal::VerifyBlasVectorSize(y, y.GetSize(), "y");
  internal::EnsureBlasMatrixSize(A, x.GetSize(), y.GetSize(), "A");

  using AT = typename Matrix::ElementType;
  const auto& x_map = x.GetMap();
  const auto& y_map = y.GetMap();
  const auto& a_map = A.GetMap();
  const auto* x_data = x.HostRead();
  const auto* y_data = y.HostRead();
  auto* a_data = A.HostReadWrite();
  for (int j = 0; j < y.GetSize(); ++j) {
    for (int i = 0; i < x.GetSize(); ++i) {
      a_data[a_map(i, j)] = static_cast<AT>(
          a_data[a_map(i, j)] + alpha * x_data[x_map(i)] * y_data[y_map(j)]);
    }
  }
}

/// @brief Triangular matrix-vector multiply in place: x = op(A) * x.
template <DenseTensorLike Matrix, DenseTensorLike Vector>
inline void Trmv(const Matrix& A, Vector& x,
                 TriangleMode uplo = TriangleMode::kLower,
                 TransposeMode trans = TransposeMode::kNoTranspose,
                 DiagonalMode diag = DiagonalMode::kNonUnit) {
  if constexpr (Matrix::GetRank() != 2) {
    ASC_VERIFY(false, "A must be a matrix");
  } else {
    const int n = A.GetExtent(0);
    internal::VerifyBlasMatrixSize(A, n, n, "A");
    internal::VerifyBlasVectorSize(x, n, "x");

    using T = typename Vector::ElementType;
    DVector<T> result(n);
    const auto& a_map = A.GetMap();
    const auto& x_map = x.GetMap();
    const auto* a_data = A.HostRead();
    const auto* x_data = x.HostRead();
    auto* result_data = result.HostWrite();

    for (int i = 0; i < n; ++i) {
      typename Matrix::ValueType sum = typename Matrix::ValueType(0);
      for (int j = 0; j < n; ++j) {
        sum += internal::TriangularOpValue(a_data, a_map, i, j, uplo, trans,
                                           diag) *
               x_data[x_map(j)];
      }
      result_data[i] = static_cast<T>(sum);
    }

    auto* x_write = x.HostWrite();
    for (int i = 0; i < n; ++i) {
      x_write[x_map(i)] = result_data[i];
    }
  }
}

/// @brief Solve a triangular system in place: op(A) * x = x.
template <DenseTensorLike Matrix, DenseTensorLike Vector>
inline void Trsv(const Matrix& A, Vector& x,
                 TriangleMode uplo = TriangleMode::kLower,
                 TransposeMode trans = TransposeMode::kNoTranspose,
                 DiagonalMode diag = DiagonalMode::kNonUnit) {
  if constexpr (Matrix::GetRank() != 2) {
    ASC_VERIFY(false, "A must be a matrix");
  } else {
    const int n = A.GetExtent(0);
    internal::VerifyBlasMatrixSize(A, n, n, "A");
    internal::VerifyBlasVectorSize(x, n, "x");

    const bool op_is_lower =
        trans == TransposeMode::kNoTranspose ? uplo == TriangleMode::kLower
                                             : uplo == TriangleMode::kUpper;
    const auto& a_map = A.GetMap();
    const auto& x_map = x.GetMap();
    const auto* a_data = A.HostRead();
    auto* x_data = x.HostReadWrite();

    if (op_is_lower) {
      for (int i = 0; i < n; ++i) {
        typename Matrix::ValueType value = x_data[x_map(i)];
        for (int j = 0; j < i; ++j) {
          value -= internal::TriangularOpValue(a_data, a_map, i, j, uplo,
                                               trans, diag) *
                   x_data[x_map(j)];
        }
        const auto diag_value = internal::TriangularOpValue(
            a_data, a_map, i, i, uplo, trans, diag);
        ASC_VERIFY(diag_value != typename Matrix::ValueType(0),
                      "Trsv: zero diagonal");
        x_data[x_map(i)] = static_cast<typename Vector::ElementType>(
            value / diag_value);
      }
    } else {
      for (int i = n - 1; i >= 0; --i) {
        typename Matrix::ValueType value = x_data[x_map(i)];
        for (int j = i + 1; j < n; ++j) {
          value -= internal::TriangularOpValue(a_data, a_map, i, j, uplo,
                                               trans, diag) *
                   x_data[x_map(j)];
        }
        const auto diag_value = internal::TriangularOpValue(
            a_data, a_map, i, i, uplo, trans, diag);
        ASC_VERIFY(diag_value != typename Matrix::ValueType(0),
                      "Trsv: zero diagonal");
        x_data[x_map(i)] = static_cast<typename Vector::ElementType>(
            value / diag_value);
      }
    }
  }
}

/// @brief General matrix multiply: C = alpha * op(A) * op(B) + beta * C.
template <DenseTensorLike MatrixA, DenseTensorLike MatrixB,
          DenseTensorLike MatrixC>
inline void Gemm(
    const MatrixA& A, const MatrixB& B, MatrixC& C,
    typename MatrixC::ValueType alpha = typename MatrixC::ValueType(1),
    typename MatrixC::ValueType beta = typename MatrixC::ValueType(0),
    TransposeMode trans_a = TransposeMode::kNoTranspose,
    TransposeMode trans_b = TransposeMode::kNoTranspose) {
  if constexpr (MatrixA::GetRank() != 2 || MatrixB::GetRank() != 2) {
    ASC_VERIFY(false, "A and B must be matrices");
  } else {
    const int a_rows = A.GetExtent(0);
    const int a_cols = A.GetExtent(1);
    const int b_rows = B.GetExtent(0);
    const int b_cols = B.GetExtent(1);
    const int m = trans_a == TransposeMode::kNoTranspose ? a_rows : a_cols;
    const int k_a = trans_a == TransposeMode::kNoTranspose ? a_cols : a_rows;
    const int k_b = trans_b == TransposeMode::kNoTranspose ? b_rows : b_cols;
    const int n = trans_b == TransposeMode::kNoTranspose ? b_cols : b_rows;
    ASC_VERIFY(k_a == k_b, "Gemm: inner dimensions mismatch");
    internal::EnsureBlasMatrixSize(C, m, n, "C");

    using CT = typename MatrixC::ElementType;
    const auto& a_map = A.GetMap();
    const auto& b_map = B.GetMap();
    const auto& c_map = C.GetMap();
    const auto* a_data = A.HostRead();
    const auto* b_data = B.HostRead();
    auto* c_data = C.HostReadWrite();

    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < m; ++i) {
        typename MatrixC::ValueType sum = typename MatrixC::ValueType(0);
        for (int k = 0; k < k_a; ++k) {
          const auto a_value =
              trans_a == TransposeMode::kNoTranspose ? a_data[a_map(i, k)]
                                                     : a_data[a_map(k, i)];
          const auto b_value =
              trans_b == TransposeMode::kNoTranspose ? b_data[b_map(k, j)]
                                                     : b_data[b_map(j, k)];
          sum += a_value * b_value;
        }
        const auto old_c =
            beta == typename MatrixC::ValueType(0)
                ? typename MatrixC::ValueType(0)
                : c_data[c_map(i, j)];
        c_data[c_map(i, j)] = static_cast<CT>(alpha * sum + beta * old_c);
      }
    }
  }
}

/// @brief Symmetric matrix-matrix multiply.
template <DenseTensorLike MatrixA, DenseTensorLike MatrixB,
          DenseTensorLike MatrixC>
inline void Symm(
    const MatrixA& A, const MatrixB& B, MatrixC& C,
    typename MatrixC::ValueType alpha = typename MatrixC::ValueType(1),
    typename MatrixC::ValueType beta = typename MatrixC::ValueType(0),
    SideMode side = SideMode::kLeft, TriangleMode uplo = TriangleMode::kLower) {
  if constexpr (MatrixA::GetRank() != 2 || MatrixB::GetRank() != 2) {
    ASC_VERIFY(false, "A and B must be matrices");
  } else {
    const int m = B.GetExtent(0);
    const int n = B.GetExtent(1);
    const int a_size = side == SideMode::kLeft ? m : n;
    internal::VerifyBlasMatrixSize(A, a_size, a_size, "A");
    internal::EnsureBlasMatrixSize(C, m, n, "C");

    using CT = typename MatrixC::ElementType;
    const auto& a_map = A.GetMap();
    const auto& b_map = B.GetMap();
    const auto& c_map = C.GetMap();
    const auto* a_data = A.HostRead();
    const auto* b_data = B.HostRead();
    auto* c_data = C.HostReadWrite();

    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < m; ++i) {
        typename MatrixC::ValueType sum = typename MatrixC::ValueType(0);
        if (side == SideMode::kLeft) {
          for (int k = 0; k < m; ++k) {
            const auto a_value =
                (uplo == TriangleMode::kLower)
                    ? (i >= k ? a_data[a_map(i, k)] : a_data[a_map(k, i)])
                    : (i <= k ? a_data[a_map(i, k)] : a_data[a_map(k, i)]);
            sum += a_value * b_data[b_map(k, j)];
          }
        } else {
          for (int k = 0; k < n; ++k) {
            const auto a_value =
                (uplo == TriangleMode::kLower)
                    ? (k >= j ? a_data[a_map(k, j)] : a_data[a_map(j, k)])
                    : (k <= j ? a_data[a_map(k, j)] : a_data[a_map(j, k)]);
            sum += b_data[b_map(i, k)] * a_value;
          }
        }
        const auto old_c =
            beta == typename MatrixC::ValueType(0)
                ? typename MatrixC::ValueType(0)
                : c_data[c_map(i, j)];
        c_data[c_map(i, j)] = static_cast<CT>(alpha * sum + beta * old_c);
      }
    }
  }
}

/// @brief Symmetric rank-k update: C = alpha * op(A) * op(A)^T + beta * C.
template <DenseTensorLike MatrixA, DenseTensorLike MatrixC>
inline void Syrk(
    const MatrixA& A, MatrixC& C,
    typename MatrixC::ValueType alpha = typename MatrixC::ValueType(1),
    typename MatrixC::ValueType beta = typename MatrixC::ValueType(0),
    TriangleMode uplo = TriangleMode::kLower,
    TransposeMode trans = TransposeMode::kNoTranspose) {
  if constexpr (MatrixA::GetRank() != 2) {
    ASC_VERIFY(false, "A must be a matrix");
  } else {
    const int n = trans == TransposeMode::kNoTranspose ? A.GetExtent(0)
                                                       : A.GetExtent(1);
    const int k = trans == TransposeMode::kNoTranspose ? A.GetExtent(1)
                                                       : A.GetExtent(0);
    internal::EnsureBlasMatrixSize(C, n, n, "C");

    using CT = typename MatrixC::ElementType;
    const auto& a_map = A.GetMap();
    const auto& c_map = C.GetMap();
    const auto* a_data = A.HostRead();
    auto* c_data = C.HostReadWrite();

    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if ((uplo == TriangleMode::kLower && i < j) ||
            (uplo == TriangleMode::kUpper && i > j)) {
          continue;
        }
        typename MatrixC::ValueType sum = typename MatrixC::ValueType(0);
        for (int l = 0; l < k; ++l) {
          const auto ai = trans == TransposeMode::kNoTranspose
                              ? a_data[a_map(i, l)]
                              : a_data[a_map(l, i)];
          const auto aj = trans == TransposeMode::kNoTranspose
                              ? a_data[a_map(j, l)]
                              : a_data[a_map(l, j)];
          sum += ai * aj;
        }
        const auto old_c =
            beta == typename MatrixC::ValueType(0)
                ? typename MatrixC::ValueType(0)
                : c_data[c_map(i, j)];
        const CT value = static_cast<CT>(alpha * sum + beta * old_c);
        c_data[c_map(i, j)] = value;
        if (i != j) c_data[c_map(j, i)] = value;
      }
    }
  }
}

/// @brief Triangular matrix-matrix multiply in place.
template <DenseTensorLike MatrixA, DenseTensorLike MatrixB>
inline void Trmm(
    const MatrixA& A, MatrixB& B,
    typename MatrixB::ValueType alpha = typename MatrixB::ValueType(1),
    SideMode side = SideMode::kLeft, TriangleMode uplo = TriangleMode::kLower,
    TransposeMode trans = TransposeMode::kNoTranspose,
    DiagonalMode diag = DiagonalMode::kNonUnit) {
  if constexpr (MatrixA::GetRank() != 2 || MatrixB::GetRank() != 2) {
    ASC_VERIFY(false, "A and B must be matrices");
  } else {
    const int m = B.GetExtent(0);
    const int n = B.GetExtent(1);
    const int a_size = side == SideMode::kLeft ? m : n;
    internal::VerifyBlasMatrixSize(A, a_size, a_size, "A");

    using T = typename MatrixB::ElementType;
    DMatrix<T> result(m, n);
    const auto& a_map = A.GetMap();
    const auto& b_map = B.GetMap();
    const auto& r_map = result.GetMap();
    const auto* a_data = A.HostRead();
    const auto* b_data = B.HostRead();
    auto* r_data = result.HostWrite();

    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < m; ++i) {
        typename MatrixB::ValueType sum = typename MatrixB::ValueType(0);
        if (side == SideMode::kLeft) {
          for (int k = 0; k < m; ++k) {
            sum += internal::TriangularOpValue(a_data, a_map, i, k, uplo,
                                               trans, diag) *
                   b_data[b_map(k, j)];
          }
        } else {
          for (int k = 0; k < n; ++k) {
            sum += b_data[b_map(i, k)] *
                   internal::TriangularOpValue(a_data, a_map, k, j, uplo,
                                               trans, diag);
          }
        }
        r_data[r_map(i, j)] = static_cast<T>(alpha * sum);
      }
    }

    auto* b_write = B.HostWrite();
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < m; ++i) {
        b_write[b_map(i, j)] = r_data[r_map(i, j)];
      }
    }
  }
}

/// @brief Triangular solve with multiple right-hand sides in place.
template <DenseTensorLike MatrixA, DenseTensorLike MatrixB>
inline void Trsm(
    const MatrixA& A, MatrixB& B,
    typename MatrixB::ValueType alpha = typename MatrixB::ValueType(1),
    SideMode side = SideMode::kLeft, TriangleMode uplo = TriangleMode::kLower,
    TransposeMode trans = TransposeMode::kNoTranspose,
    DiagonalMode diag = DiagonalMode::kNonUnit) {
  if constexpr (MatrixA::GetRank() != 2 || MatrixB::GetRank() != 2) {
    ASC_VERIFY(false, "A and B must be matrices");
  } else {
    const int m = B.GetExtent(0);
    const int n = B.GetExtent(1);
    const int a_size = side == SideMode::kLeft ? m : n;
    internal::VerifyBlasMatrixSize(A, a_size, a_size, "A");

    using T = typename MatrixB::ElementType;
    const auto& b_map = B.GetMap();
    auto* b_data = B.HostReadWrite();

    if (side == SideMode::kLeft) {
      DVector<T> rhs(m);
      for (int col = 0; col < n; ++col) {
        auto* rhs_data = rhs.HostWrite();
        for (int i = 0; i < m; ++i) {
          rhs_data[i] = static_cast<T>(alpha * b_data[b_map(i, col)]);
        }
        Trsv(A, rhs, uplo, trans, diag);
        rhs_data = rhs.HostReadWrite();
        for (int i = 0; i < m; ++i) {
          b_data[b_map(i, col)] = rhs_data[i];
        }
      }
    } else {
      DVector<T> rhs(n);
      const TransposeMode row_trans = internal::ToggleTranspose(trans);
      for (int row = 0; row < m; ++row) {
        auto* rhs_data = rhs.HostWrite();
        for (int j = 0; j < n; ++j) {
          rhs_data[j] = static_cast<T>(alpha * b_data[b_map(row, j)]);
        }
        Trsv(A, rhs, uplo, row_trans, diag);
        rhs_data = rhs.HostReadWrite();
        for (int j = 0; j < n; ++j) {
          b_data[b_map(row, j)] = rhs_data[j];
        }
      }
    }
  }
}

/// @brief Matrix multiplication: C = A @ B
/// @param A Left matrix (M x K)
/// @param B Right matrix (K x N)
/// @param C Output matrix (M x N)
template <DenseMatrixLike Matrix>
inline void MatMul(const Matrix& A, const Matrix& B, Matrix& C) {
  Gemm(A, B, C);
}

/// @brief Matrix-vector multiplication: y = A @ x
/// @param A Matrix (M x N)
/// @param x Vector (length N)
/// @param y Output vector (length M)
template <DenseMatrixLike Matrix, DenseVectorLike Vector>
inline void MatMul(const Matrix& A, const Vector& x, Vector& y) {
  Gemv(A, x, y);
}

/// @brief Outer product of two vectors: C[i,j] = a[i] * b[j]
/// @param a First vector (length M)
/// @param b Second vector (length N)
/// @param C Output matrix (M x N)
template <DenseVectorLike Vector, DenseMatrixLike Matrix>
inline void Outer(const Vector& a, const Vector& b, Matrix& C) {
  using T = typename Vector::ValueType;
  internal::VerifyBlasVectorSize(a, a.GetSize(), "a");
  internal::VerifyBlasVectorSize(b, b.GetSize(), "b");
  internal::EnsureBlasMatrixSize(C, a.GetSize(), b.GetSize(), "C");

  const auto& c_map = C.GetMap();
  auto* c_data = C.HostWrite();
  for (int j = 0; j < b.GetSize(); ++j) {
    for (int i = 0; i < a.GetSize(); ++i) {
      c_data[c_map(i, j)] = T(0);
    }
  }
  Ger(T(1), a, b, C);
}

// ============================================================================
// Category 12: Tensor Contraction Operations
// ============================================================================

/// @brief Tensor dot product over specified axes (generalized contraction)
/// @param A First tensor
/// @param B Second tensor
/// @param axes_a Axes to contract in tensor A
/// @param axes_b Axes to contract in tensor B
/// @param C Output tensor
///
/// @note This is a generalization of matrix multiplication to arbitrary
/// dimensions. Contracts specified axes between two tensors by summing over
/// matching indices.
///
/// @par Examples:
/// - TensorDot(A, B, {1}, {0}, C): Matrix multiplication (contract last axis
/// of A with first of B)
/// - TensorDot(A, B, {1,2}, {0,1}, C): Contract axes 1,2 of A with axes 0,1 of
/// B
///
/// @par Algorithm:
/// 1. Validate contraction axes have matching dimensions
/// 2. Compute output shape (free axes from A followed by free axes from B)
/// 3. Perform contraction via nested loops over all axes
///
/// @par Complexity:
/// O(product of all dimensions) - can be expensive for high-dimensional
/// tensors
template <TensorLike TensorA, TensorLike TensorB, TensorLike TensorC>
inline void TensorDot(const TensorA& A, const TensorB& B,
                      const UArray<int>& axes_a, const UArray<int>& axes_b,
                      TensorC& C) {
  // Validate axes
  ASC_ASSERT(axes_a.GetSize() == axes_b.GetSize(),
                "number of contraction axes must match");
  const int num_contract = axes_a.GetSize();
  const int rank_a = A.GetRank();
  const int rank_b = B.GetRank();

  // Validate contraction dimensions match
  const int* axes_a_data = axes_a.Read();
  const int* axes_b_data = axes_b.Read();
  for (int i = 0; i < num_contract; i++) {
    ASC_ASSERT(
        axes_a_data[i] >= 0 && axes_a_data[i] < rank_a,
        "contraction axis in A out of range");
    ASC_ASSERT(
        axes_b_data[i] >= 0 && axes_b_data[i] < rank_b,
        "contraction axis in B out of range");
    ASC_ASSERT(A.GetExtent(axes_a_data[i]) == B.GetExtent(axes_b_data[i]),
                  "contraction dimension mismatch");
  }

  // Compute output size and initialize to zero
  const int C_size = C.GetSize();
  using T = typename TensorC::ValueType;
  T* Cdata = C.ReadWrite();
  const bool use_dev = A.UseDevice();
  C.UseDevice(use_dev);
  ASC_FORALL_SWITCH(use_dev, i, C_size, Cdata[i] = T(0););

  // Get input data
  const T* Adata = A.Read();
  const T* Bdata = B.Read();

  // Build free axis lists
  UArray<int> free_a(rank_a - num_contract);
  UArray<int> free_b(rank_b - num_contract);
  int* free_a_data = free_a.HostReadWrite();
  int* free_b_data = free_b.HostReadWrite();

  int free_a_idx = 0;
  for (int i = 0; i < rank_a; i++) {
    bool is_contracted = false;
    for (int j = 0; j < num_contract; j++) {
      if (i == axes_a_data[j]) {
        is_contracted = true;
        break;
      }
    }
    if (!is_contracted) {
      free_a_data[free_a_idx++] = i;
    }
  }

  int free_b_idx = 0;
  for (int i = 0; i < rank_b; i++) {
    bool is_contracted = false;
    for (int j = 0; j < num_contract; j++) {
      if (i == axes_b_data[j]) {
        is_contracted = true;
        break;
      }
    }
    if (!is_contracted) {
      free_b_data[free_b_idx++] = i;
    }
  }

  // Perform contraction (on host for simplicity)
  const int num_free_a = free_a.GetSize();
  const int num_free_b = free_b.GetSize();

  // For each element in output C
  for (int c_idx = 0; c_idx < C_size; c_idx++) {
    // Determine multi-index in C (column-major: first dimension varies fastest)
    UArray<int> c_multi(num_free_a + num_free_b);
    int* c_multi_data = c_multi.HostReadWrite();
    int temp = c_idx;
    for (int i = 0; i < num_free_a + num_free_b; i++) {
      int extent = (i < num_free_a) ? A.GetExtent(free_a_data[i])
                                    : B.GetExtent(free_b_data[i - num_free_a]);
      c_multi_data[i] = temp % extent;
      temp /= extent;
    }

    // Sum over contracted indices
    T sum = T(0);

    // Compute product of contracted dimensions
    int contract_size = 1;
    for (int i = 0; i < num_contract; i++) {
      contract_size *= A.GetExtent(axes_a_data[i]);
    }

    for (int k = 0; k < contract_size; k++) {
      // Build multi-index for contracted axes (column-major order)
      UArray<int> contract_multi(num_contract);
      int* contract_multi_data = contract_multi.HostReadWrite();
      int temp_k = k;
      for (int i = 0; i < num_contract; i++) {
        int extent = A.GetExtent(axes_a_data[i]);
        contract_multi_data[i] = temp_k % extent;
        temp_k /= extent;
      }

      // Build full multi-index for A
      UArray<int> a_multi(rank_a);
      int* a_multi_data = a_multi.HostReadWrite();
      for (int i = 0; i < num_free_a; i++) {
        a_multi_data[free_a_data[i]] = c_multi_data[i];
      }
      for (int i = 0; i < num_contract; i++) {
        a_multi_data[axes_a_data[i]] = contract_multi_data[i];
      }

      // Build full multi-index for B
      UArray<int> b_multi(rank_b);
      int* b_multi_data = b_multi.HostReadWrite();
      for (int i = 0; i < num_free_b; i++) {
        b_multi_data[free_b_data[i]] = c_multi_data[num_free_a + i];
      }
      for (int i = 0; i < num_contract; i++) {
        b_multi_data[axes_b_data[i]] = contract_multi_data[i];
      }

      // Convert multi-index to linear index (column-major / LayoutLeft)
      int a_idx = 0;
      int a_stride = 1;
      for (int i = 0; i < rank_a; i++) {
        a_idx += a_multi_data[i] * a_stride;
        a_stride *= A.GetExtent(i);
      }

      int b_idx = 0;
      int b_stride = 1;
      for (int i = 0; i < rank_b; i++) {
        b_idx += b_multi_data[i] * b_stride;
        b_stride *= B.GetExtent(i);
      }

      sum += Adata[a_idx] * Bdata[b_idx];
    }

    Cdata[c_idx] = sum;
  }
}

/// @brief Tensor product (Kronecker product) for 2D matrices
/// @param A First matrix (M_a x N_a)
/// @param B Second matrix (M_b x N_b)
/// @param C Output matrix (size: (M_a*M_b) x (N_a*N_b))
///
/// @note For matrices A (M_a x N_a) and B (M_b x N_b), computes:
/// C[i*M_b+k, j*N_b+l] = A[i,j] * B[k,l]
template <MatrixLike Matrix>
inline void Kron(const Matrix& A, const Matrix& B, Matrix& C) {
  ASC_ASSERT(A.GetRank() == 2 && B.GetRank() == 2 && C.GetRank() == 2,
                "Kron requires 2D matrices");

  const int Ma = A.GetExtent(0);
  const int Na = A.GetExtent(1);
  const int Mb = B.GetExtent(0);
  const int Nb = B.GetExtent(1);
  const int Mc = Ma * Mb;
  const int Nc = Na * Nb;

  ASC_ASSERT(C.GetExtent(0) == Mc && C.GetExtent(1) == Nc,
                "output matrix has wrong dimensions");

  using T = typename Matrix::ValueType;
  const T* Adata = A.Read();
  const T* Bdata = B.Read();
  T* Cdata = C.ReadWrite();
  const bool use_dev = A.UseDevice();
  C.UseDevice(use_dev);

  const int total = Mc * Nc;
  if constexpr (std::is_same_v<DefaultLayout, LayoutLeft>) {
    // Column-major layout
    ASC_FORALL_SWITCH(use_dev, idx, total, {
      const int i = idx % Mc;  // Row index in C
      const int j = idx / Mc;  // Column index in C
      const int ia = i / Mb;   // Row index in A
      const int ib = i % Mb;   // Row index in B
      const int ja = j / Nb;   // Column index in A
      const int jb = j % Nb;   // Column index in B
      Cdata[j * Mc + i] = Adata[ja * Ma + ia] * Bdata[jb * Mb + ib];
    });
  } else {
    // Row-major layout
    ASC_FORALL_SWITCH(use_dev, idx, total, {
      const int i = idx / Nc;  // Row index in C
      const int j = idx % Nc;  // Column index in C
      const int ia = i / Mb;   // Row index in A
      const int ib = i % Mb;   // Row index in B
      const int ja = j / Nb;   // Column index in A
      const int jb = j % Nb;   // Column index in B
      Cdata[i * Nc + j] = Adata[ia * Na + ja] * Bdata[ib * Nb + jb];
    });
  }
}

/// @brief Cross product for 3D vectors
/// @param a First vector (length 3)
/// @param b Second vector (length 3)
/// @param c Output vector (length 3)
///
/// @note Computes c = a × b using the standard cross product formula
template <VectorLike Vector>
inline void Cross(const Vector& a, const Vector& b, Vector& c) {
  ASC_ASSERT(a.GetSize() == 3 && b.GetSize() == 3 && c.GetSize() == 3,
                "Cross product requires 3D vectors");

  using T = typename Vector::ValueType;
  const T* adata = a.Read();
  const T* bdata = b.Read();
  T* cdata = c.ReadWrite();
  const bool use_dev = a.UseDevice();
  c.UseDevice(use_dev);

  // Cross product: c = a × b
  // c[0] = a[1]*b[2] - a[2]*b[1]
  // c[1] = a[2]*b[0] - a[0]*b[2]
  // c[2] = a[0]*b[1] - a[1]*b[0]

  T temp[3];
  temp[0] = adata[1] * bdata[2] - adata[2] * bdata[1];
  temp[1] = adata[2] * bdata[0] - adata[0] * bdata[2];
  temp[2] = adata[0] * bdata[1] - adata[1] * bdata[0];

  for (int i = 0; i < 3; i++) {
    cdata[i] = temp[i];
  }
}

}  // namespace asc

#endif  // ASC_BLAS_H_
