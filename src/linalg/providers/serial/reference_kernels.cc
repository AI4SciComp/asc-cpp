// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include "asc/linalg/detail/reference_kernels.h"

#include <cmath>
#include <limits>

namespace asc::detail {

namespace {

template <typename T>
void CopyKernel(const T* x, stride_t x_stride, T* y, stride_t y_stride,
                extent_t size) noexcept {
  for (extent_t index = 0; index < size; ++index) {
    y[index * y_stride] = x[index * x_stride];
  }
}

template <typename T>
void ScalKernel(T alpha, T* x, stride_t x_stride, extent_t size) noexcept {
  for (extent_t index = 0; index < size; ++index) {
    x[index * x_stride] = alpha * x[index * x_stride];
  }
}

template <typename T>
void AxpyKernel(T alpha, const T* x, stride_t x_stride, T* y,
                stride_t y_stride, extent_t size) noexcept {
  for (extent_t index = 0; index < size; ++index) {
    y[index * y_stride] =
        alpha * x[index * x_stride] + y[index * y_stride];
  }
}

template <typename T>
T DotKernel(const T* x, stride_t x_stride, const T* y, stride_t y_stride,
            extent_t size) noexcept {
  T result = T{0};
  for (extent_t index = 0; index < size; ++index) {
    result += x[index * x_stride] * y[index * y_stride];
  }
  return result;
}

template <typename T>
T Nrm2Kernel(const T* x, stride_t x_stride, extent_t size) noexcept {
  T scale = T{0};
  T sum_of_squares = T{1};
  bool has_infinity = false;
  for (extent_t index = 0; index < size; ++index) {
    const T magnitude = std::abs(x[index * x_stride]);
    if (std::isnan(magnitude)) {
      return std::numeric_limits<T>::quiet_NaN();
    }
    if (std::isinf(magnitude)) {
      has_infinity = true;
      continue;
    }
    if (magnitude == T{0}) {
      continue;
    }
    if (scale < magnitude) {
      const T ratio = scale / magnitude;
      sum_of_squares = T{1} + sum_of_squares * ratio * ratio;
      scale = magnitude;
    } else {
      const T ratio = magnitude / scale;
      sum_of_squares += ratio * ratio;
    }
  }
  if (has_infinity) {
    return std::numeric_limits<T>::infinity();
  }
  if (scale == T{0}) {
    return T{0};
  }
  return scale * std::sqrt(sum_of_squares);
}

template <typename T>
void GemvKernel(TransposeMode transpose, T alpha, const T* matrix,
                stride_t row_stride, stride_t column_stride, extent_t rows,
                extent_t columns, const T* x, stride_t x_stride, T beta, T* y,
                stride_t y_stride) noexcept {
  const extent_t output_size = transpose == TransposeMode::kNoTranspose
                                   ? rows
                                   : columns;
  const extent_t inner_size = transpose == TransposeMode::kNoTranspose
                                  ? columns
                                  : rows;
  for (extent_t output = 0; output < output_size; ++output) {
    T sum = T{0};
    for (extent_t inner = 0; inner < inner_size; ++inner) {
      const extent_t matrix_offset =
          transpose == TransposeMode::kNoTranspose
              ? output * row_stride + inner * column_stride
              : inner * row_stride + output * column_stride;
      sum += matrix[matrix_offset] * x[inner * x_stride];
    }
    const extent_t output_offset = output * y_stride;
    if (beta == T{0}) {
      y[output_offset] = alpha * sum;
    } else {
      y[output_offset] = alpha * sum + beta * y[output_offset];
    }
  }
}

template <typename T>
void GemmKernel(TransposeMode transpose_a, TransposeMode transpose_b, T alpha,
                const T* a, stride_t a_row_stride,
                stride_t a_column_stride, extent_t a_rows,
                extent_t a_columns, const T* b, stride_t b_row_stride,
                stride_t b_column_stride, extent_t b_rows,
                extent_t b_columns, T beta, T* c, stride_t c_row_stride,
                stride_t c_column_stride) noexcept {
  const extent_t output_rows =
      transpose_a == TransposeMode::kNoTranspose ? a_rows : a_columns;
  const extent_t inner_size =
      transpose_a == TransposeMode::kNoTranspose ? a_columns : a_rows;
  const extent_t output_columns =
      transpose_b == TransposeMode::kNoTranspose ? b_columns : b_rows;
  for (extent_t column = 0; column < output_columns; ++column) {
    for (extent_t row = 0; row < output_rows; ++row) {
      T sum = T{0};
      for (extent_t inner = 0; inner < inner_size; ++inner) {
        const extent_t a_offset =
            transpose_a == TransposeMode::kNoTranspose
                ? row * a_row_stride + inner * a_column_stride
                : inner * a_row_stride + row * a_column_stride;
        const extent_t b_offset =
            transpose_b == TransposeMode::kNoTranspose
                ? inner * b_row_stride + column * b_column_stride
                : column * b_row_stride + inner * b_column_stride;
        sum += a[a_offset] * b[b_offset];
      }
      const extent_t c_offset =
          row * c_row_stride + column * c_column_stride;
      if (beta == T{0}) {
        c[c_offset] = alpha * sum;
      } else {
        c[c_offset] = alpha * sum + beta * c[c_offset];
      }
    }
  }
}

}  // namespace

void ReferenceCopy(const float* x, stride_t x_stride, float* y,
                   stride_t y_stride, extent_t size) noexcept {
  CopyKernel(x, x_stride, y, y_stride, size);
}

void ReferenceCopy(const double* x, stride_t x_stride, double* y,
                   stride_t y_stride, extent_t size) noexcept {
  CopyKernel(x, x_stride, y, y_stride, size);
}

void ReferenceScal(float alpha, float* x, stride_t x_stride,
                   extent_t size) noexcept {
  ScalKernel(alpha, x, x_stride, size);
}

void ReferenceScal(double alpha, double* x, stride_t x_stride,
                   extent_t size) noexcept {
  ScalKernel(alpha, x, x_stride, size);
}

void ReferenceAxpy(float alpha, const float* x, stride_t x_stride, float* y,
                   stride_t y_stride, extent_t size) noexcept {
  AxpyKernel(alpha, x, x_stride, y, y_stride, size);
}

void ReferenceAxpy(double alpha, const double* x, stride_t x_stride, double* y,
                   stride_t y_stride, extent_t size) noexcept {
  AxpyKernel(alpha, x, x_stride, y, y_stride, size);
}

float ReferenceDot(const float* x, stride_t x_stride, const float* y,
                   stride_t y_stride, extent_t size) noexcept {
  return DotKernel(x, x_stride, y, y_stride, size);
}

double ReferenceDot(const double* x, stride_t x_stride, const double* y,
                    stride_t y_stride, extent_t size) noexcept {
  return DotKernel(x, x_stride, y, y_stride, size);
}

float ReferenceNrm2(const float* x, stride_t x_stride,
                    extent_t size) noexcept {
  return Nrm2Kernel(x, x_stride, size);
}

double ReferenceNrm2(const double* x, stride_t x_stride,
                     extent_t size) noexcept {
  return Nrm2Kernel(x, x_stride, size);
}

void ReferenceGemv(TransposeMode transpose, float alpha, const float* matrix,
                   stride_t row_stride, stride_t column_stride, extent_t rows,
                   extent_t columns, const float* x, stride_t x_stride,
                   float beta, float* y, stride_t y_stride) noexcept {
  GemvKernel(transpose, alpha, matrix, row_stride, column_stride, rows, columns,
             x, x_stride, beta, y, y_stride);
}

void ReferenceGemv(TransposeMode transpose, double alpha, const double* matrix,
                   stride_t row_stride, stride_t column_stride, extent_t rows,
                   extent_t columns, const double* x, stride_t x_stride,
                   double beta, double* y, stride_t y_stride) noexcept {
  GemvKernel(transpose, alpha, matrix, row_stride, column_stride, rows, columns,
             x, x_stride, beta, y, y_stride);
}

void ReferenceGemm(TransposeMode transpose_a, TransposeMode transpose_b,
                   float alpha, const float* a, stride_t a_row_stride,
                   stride_t a_column_stride, extent_t a_rows,
                   extent_t a_columns, const float* b, stride_t b_row_stride,
                   stride_t b_column_stride, extent_t b_rows,
                   extent_t b_columns, float beta, float* c,
                   stride_t c_row_stride, stride_t c_column_stride) noexcept {
  GemmKernel(transpose_a, transpose_b, alpha, a, a_row_stride, a_column_stride,
             a_rows, a_columns, b, b_row_stride, b_column_stride, b_rows,
             b_columns, beta, c, c_row_stride, c_column_stride);
}

void ReferenceGemm(TransposeMode transpose_a, TransposeMode transpose_b,
                   double alpha, const double* a, stride_t a_row_stride,
                   stride_t a_column_stride, extent_t a_rows,
                   extent_t a_columns, const double* b, stride_t b_row_stride,
                   stride_t b_column_stride, extent_t b_rows,
                   extent_t b_columns, double beta, double* c,
                   stride_t c_row_stride, stride_t c_column_stride) noexcept {
  GemmKernel(transpose_a, transpose_b, alpha, a, a_row_stride, a_column_stride,
             a_rows, a_columns, b, b_row_stride, b_column_stride, b_rows,
             b_columns, beta, c, c_row_stride, c_column_stride);
}

}  // namespace asc::detail
