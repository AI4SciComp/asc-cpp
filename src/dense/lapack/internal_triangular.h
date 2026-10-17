#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_H_

#include <algorithm>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "internal_indefinite.h"
#include "internal_triangular_counts.h"

namespace asc::internal_triangular {
namespace common = internal_indefinite;

inline bool Diagonal(DenseBlasDiagonal diagonal) {
  return diagonal == DenseBlasDiagonal::kUnit ||
         diagonal == DenseBlasDiagonal::kNonUnit;
}
inline bool Operation(DenseBlasTranspose operation) {
  return operation == DenseBlasTranspose::kNone ||
         operation == DenseBlasTranspose::kTranspose ||
         operation == DenseBlasTranspose::kConjugateTranspose;
}
inline char Diag(DenseBlasDiagonal diagonal) {
  return diagonal == DenseBlasDiagonal::kUnit ? 'U' : 'N';
}
inline char Trans(DenseBlasTranspose operation) {
  if (operation == DenseBlasTranspose::kNone) {
    return 'N';
  }
  return operation == DenseBlasTranspose::kTranspose ? 'T' : 'C';
}

template <typename T>
extent_t SolveLeading(DenseBlasMatrixView<T> a, extent_t nrhs,
                      DenseBlasDiagonal diagonal) {
  if (nrhs != 0 || a.rows() == 0) {
    return common::Leading(a);
  }
  // With no RHS, only a nonunit diagonal can be read. Its address is
  // i*(ld+1) in either square layout, so original row storage is valid.
  return diagonal == DenseBlasDiagonal::kUnit || a.rows() == 1
             ? a.rows()
             : a.leading_dimension();
}

template <typename T>
T* Pack(DenseBlasMatrixView<T> a, DenseBlasTriangle triangle,
        DenseBlasDiagonal diagonal, std::remove_const_t<T>*& cursor) {
  if (!common::Packed(a) || a.rows() == 0) {
    return a.data();
  }
  auto* packed = cursor;
  cursor += a.rows() * a.columns();
  for (extent_t j = 0; j < a.columns(); ++j) {
    const extent_t first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const extent_t last =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : a.rows();
    for (extent_t i = first; i < last; ++i) {
      if (i != j || diagonal == DenseBlasDiagonal::kNonUnit) {
        packed[j * a.rows() + i] = common::Entry(a, i, j);
      }
    }
  }
  return packed;
}

template <typename T>
void Publish(const T* packed, DenseBlasMatrixView<T> a,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal) {
  if (!common::Packed(a)) {
    return;
  }
  for (extent_t j = 0; j < a.columns(); ++j) {
    const extent_t first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const extent_t last =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : a.rows();
    for (extent_t i = first; i < last; ++i) {
      if (i != j || diagonal == DenseBlasDiagonal::kNonUnit) {
        common::Entry(a, i, j) = packed[j * a.rows() + i];
      }
    }
  }
}

inline Status Info(lapack_int info, extent_t n, bool singular_allowed,
                   LapackReport& report) {
  report.native_info = info;
  if (info == 0) {
    return common::Complete(report);
  }
  if (info > 0 && info <= n && singular_allowed) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kUnchanged;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  return common::Defect(info, report);
}

}  // namespace asc::internal_triangular

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_H_
