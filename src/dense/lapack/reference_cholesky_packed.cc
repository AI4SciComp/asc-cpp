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
#include "asc/dense/providers/lapack_cholesky_packed.h"
#include "internal_indefinite.h"
#include "internal_packed_cholesky_counts.h"
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
  static constexpr std::string_view kRoutine = "spptrf";
  static lapack_int Factor(char uplo, lapack_int n, float* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_spptrf(&uplo, &n, a, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kRoutine = "dpptrf";
  static lapack_int Factor(char uplo, lapack_int n, double* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dpptrf(&uplo, &n, a, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kRoutine = "cpptrf";
  static lapack_int Factor(char uplo, lapack_int n, std::complex<float>* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cpptrf(&uplo, &n, a, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kRoutine = "zpptrf";
  static lapack_int Factor(char uplo, lapack_int n, std::complex<double>* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zpptrf(&uplo, &n, a, &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasPackedMatrixView<T> a) {
  if (!common::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  Status status = common::Accessible(provider, a.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  status = internal_packed_cholesky_counts::Factor(a.order(), triangle,
                                                   common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kRoutine, Native<T>::kKind, std::array{a.order()},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(a.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  status = packed::Packing(a, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan) : status;
}

template <typename T>
T* Pack(DenseBlasPackedMatrixView<T> a, DenseBlasTriangle triangle,
        const LapackWorkspace& workspace) {
  if (a.layout() == DenseBlasLayout::kColumnMajor) {
    return a.data();
  }
  auto* result = static_cast<T*>(workspace.regions[common::kLayout].data());
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
Status Factor(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasPackedMatrixView<T> a,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  const std::array operands{a.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kRoutine, report);
  auto expected = Query(provider, triangle, a);
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
  auto* values = Pack(a, triangle, workspace);
  report.called_provider = true;
  const auto info = Native<T>::Factor(
      common::Uplo(triangle), static_cast<lapack_int>(a.order()), values);
  report.native_info = info;
  if (info < 0 || info > a.order()) {
    return common::Defect(info, report);
  }
  Publish(values, a, triangle, info);
  if (info > 0) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> a) {
  return Query(provider, triangle, a);
}
Status Pptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasPackedMatrixView<float> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, triangle, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> a) {
  return Query(provider, triangle, a);
}
Status Pptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasPackedMatrixView<double> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, triangle, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> a) {
  return Query(provider, triangle, a);
}
Status Pptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<float>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, triangle, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> a) {
  return Query(provider, triangle, a);
}
Status Pptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<double>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, triangle, a, plan, workspace, report);
}

}  // namespace asc
