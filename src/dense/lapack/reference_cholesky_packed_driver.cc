#include <algorithm>
#include <array>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_driver.h"
#include "internal_indefinite.h"
#include "internal_packed_cholesky_driver_counts.h"
#include "internal_packed_triangular.h"

namespace asc {
namespace {
namespace common = internal_indefinite;
namespace packed = internal_packed_triangular;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kRoutine = "sppsv";
  static lapack_int Solve(char uplo, lapack_int n, lapack_int nrhs, float* a,
                          float* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sppsv(&uplo, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kRoutine = "dppsv";
  static lapack_int Solve(char uplo, lapack_int n, lapack_int nrhs, double* a,
                          double* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dppsv(&uplo, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kRoutine = "cppsv";
  static lapack_int Solve(char uplo, lapack_int n, lapack_int nrhs,
                          std::complex<float>* a, std::complex<float>* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cppsv(&uplo, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kRoutine = "zppsv";
  static lapack_int Solve(char uplo, lapack_int n, lapack_int nrhs,
                          std::complex<double>* a, std::complex<double>* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zppsv(&uplo, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasPackedMatrixView<T> a,
                                  DenseBlasMatrixView<T> b) {
  if (!common::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (b.rows() != a.order()) {
    return Status(ErrorCode::kShape);
  }
  const std::array operands{a.reachable_storage(), b.reachable_storage()};
  for (const auto operand : operands) {
    Status status = common::Accessible(provider, operand);
    if (!status.ok()) {
      return status;
    }
  }
  Status status = common::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  const extent_t ldb =
      b.columns() == 0 ? std::max<extent_t>(1, b.rows()) : common::Leading(b);
  status = internal_packed_cholesky_driver_counts::Driver(
      a.order(), b.columns(), ldb, triangle, common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kRoutine, Native<T>::kKind,
      std::array{a.order(), b.columns(), ldb},
      std::array<std::int64_t, 4>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(a.layout()),
                                  static_cast<std::int64_t>(b.layout()),
                                  b.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (a.order() == 0) {
    return plan;
  }
  status = packed::Packing(a, plan);
  if (!status.ok()) {
    return status;
  }
  status = common::Packing(b, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan) : status;
}

template <typename T>
T* Pack(DenseBlasPackedMatrixView<T> a, DenseBlasTriangle triangle,
        T*& cursor) {
  if (a.layout() == DenseBlasLayout::kColumnMajor) {
    return a.data();
  }
  auto* result = cursor;
  cursor += packed::Entries(a.order());
  for (extent_t j = 0; j < a.order(); ++j) {
    const auto first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto last = triangle == DenseBlasTriangle::kUpper ? j + 1 : a.order();
    for (extent_t i = first; i < last; ++i) {
      const auto source = packed::Offset(a.order(), triangle, a.layout(), i, j);
      const auto target = packed::Offset(a.order(), triangle,
                                         DenseBlasLayout::kColumnMajor, i, j);
      if constexpr (DenseBlasComplex<T>) {
        if (i == j) {
          result[target] = T{a.data()[source].real(), 0};
          continue;
        }
      }
      result[target] = a.data()[source];
    }
  }
  return result;
}

template <typename T>
void Publish(const T* values, DenseBlasPackedMatrixView<T> a,
             DenseBlasTriangle triangle, lapack_int info) {
  if (a.layout() == DenseBlasLayout::kColumnMajor) {
    return;
  }
  const bool upper = triangle == DenseBlasTriangle::kUpper;
  if (!upper && info == 1) {
    a.data()[0] = values[0];
    return;
  }
  const extent_t columns = upper && info > 0 ? info : a.order();
  for (extent_t j = 0; j < columns; ++j) {
    const auto first = upper ? 0 : j;
    const auto last = upper ? j + 1 : a.order();
    for (extent_t i = first; i < last; ++i) {
      a.data()[packed::Offset(a.order(), triangle, a.layout(), i, j)] =
          values[packed::Offset(a.order(), triangle,
                                DenseBlasLayout::kColumnMajor, i, j)];
    }
  }
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasPackedMatrixView<T> a,
             DenseBlasMatrixView<T> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{a.reachable_storage(), b.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kRoutine, report);
  auto expected = Query(provider, triangle, a, b);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (a.order() == 0) {
    return common::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  auto* ap = Pack(a, triangle, cursor);
  auto* rhs = common::PackRhs(b, cursor);
  report.called_provider = true;
  const auto info = Native<T>::Solve(
      common::Uplo(triangle), static_cast<lapack_int>(a.order()),
      static_cast<lapack_int>(b.columns()), ap, rhs,
      static_cast<lapack_int>(b.columns() == 0 ? std::max<extent_t>(1, b.rows())
                                               : common::Leading(b)));
  report.native_info = info;
  if (info < 0 || info > a.order()) {
    return common::Defect(info, report);
  }
  Publish(ap, a, triangle, info);
  if (info > 0) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(ErrorCode::kNumerical);
  }
  common::PublishRhs(rhs, b);
  return common::Complete(report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPpsvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> a, DenseBlasMatrixView<float> b) {
  return Query(provider, triangle, a, b);
}
Status Ppsv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasPackedMatrixView<float> a, DenseBlasMatrixView<float> b,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Solve(provider, triangle, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpsvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> a, DenseBlasMatrixView<double> b) {
  return Query(provider, triangle, a, b);
}
Status Ppsv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasPackedMatrixView<double> a, DenseBlasMatrixView<double> b,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Solve(provider, triangle, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpsvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b) {
  return Query(provider, triangle, a, b);
}
Status Ppsv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasPackedMatrixView<std::complex<float>> a,
            DenseBlasMatrixView<std::complex<float>> b,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Solve(provider, triangle, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpsvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b) {
  return Query(provider, triangle, a, b);
}
Status Ppsv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasPackedMatrixView<std::complex<double>> a,
            DenseBlasMatrixView<std::complex<double>> b,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Solve(provider, triangle, a, b, plan, workspace, report);
}

}  // namespace asc
