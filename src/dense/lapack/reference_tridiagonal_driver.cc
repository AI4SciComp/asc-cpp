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
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_tridiagonal_driver.h"
#include "internal_layout.h"
#include "internal_tridiagonal.h"
#include "internal_tridiagonal_counts.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr std::string_view kName = "sgtsv";
  static lapack_int Execute(lapack_int n, lapack_int nrhs, float* lower,
                            float* diagonal, float* upper, float* rhs,
                            lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgtsv(&n, &nrhs, lower, diagonal, upper, rhs, &ldb, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr std::string_view kName = "dgtsv";
  static lapack_int Execute(lapack_int n, lapack_int nrhs, double* lower,
                            double* diagonal, double* upper, double* rhs,
                            lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgtsv(&n, &nrhs, lower, diagonal, upper, rhs, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr std::string_view kName = "cgtsv";
  static lapack_int Execute(lapack_int n, lapack_int nrhs,
                            std::complex<float>* lower,
                            std::complex<float>* diagonal,
                            std::complex<float>* upper,
                            std::complex<float>* rhs, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgtsv(&n, &nrhs, lower, diagonal, upper, rhs, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr std::string_view kName = "zgtsv";
  static lapack_int Execute(lapack_int n, lapack_int nrhs,
                            std::complex<double>* lower,
                            std::complex<double>* diagonal,
                            std::complex<double>* upper,
                            std::complex<double>* rhs, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgtsv(&n, &nrhs, lower, diagonal, upper, rhs, &ldb, &info);
    return info;
  }
};

template <typename T>
auto Operands(LapackTridiagonalView<T> matrix, DenseBlasMatrixView<T> rhs) {
  const auto spans = checked::Spans(matrix);
  return std::array{spans[0], spans[1], spans[2], rhs.reachable_storage()};
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  LapackTridiagonalView<T> matrix,
                                  DenseBlasMatrixView<T> rhs) {
  const extent_t order = matrix.order();
  Status status = checked::Storage(provider, matrix);
  if (!status.ok()) {
    return status;
  }
  status = checked::Matrix(provider, rhs, order);
  if (!status.ok()) {
    return status;
  }
  status = checked::Disjoint(Operands(matrix, rhs));
  if (!status.ok()) {
    return status;
  }
  status = internal_tridiagonal_counts::Driver(order, rhs.columns(),
                                               checked::kLimit);
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kName, checked::Kind<T>(),
      std::array{order, rhs.columns(), checked::Leading(rhs)},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  status = internal_lapack_layout::AddPacking(rhs, plan);
  if (!status.ok()) {
    return status;
  }
  if constexpr (DenseBlasReal<T>) {
    if (order != 0 && rhs.columns() == 0) {
      plan.regions[checked::kScratch] = {order, order, sizeof(T), alignof(T)};
    }
  }
  status = internal_lapack_layout::CheckTotal(plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackTridiagonalView<T> matrix, DenseBlasMatrixView<T> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = Operands(matrix, rhs);
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kName, report);
  const auto expected = Query(provider, matrix, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const extent_t order = matrix.order();
  if (order == 0) {
    return checked::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  T* packed = internal_lapack_layout::Pack(rhs, cursor);
  T lower_dummy{};
  T upper_dummy{};
  T rhs_dummy{};
  if constexpr (DenseBlasReal<T>) {
    if (rhs.columns() == 0) {
      // Pinned S/DGTSV uses B(:,1) even with NRHS=0. Keep the actual count,
      // initialize explicit caller scratch, and never address public empty B.
      packed = static_cast<T*>(workspace.regions[checked::kScratch].data());
      std::fill_n(packed, order, T{});
    }
  }
  packed = checked::Nonnull(packed, rhs_dummy);
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      static_cast<lapack_int>(order), static_cast<lapack_int>(rhs.columns()),
      checked::Nonnull(matrix.lower().data(), lower_dummy),
      matrix.diagonal().data(),
      checked::Nonnull(matrix.upper().data(), upper_dummy), packed,
      static_cast<lapack_int>(checked::Leading(rhs)));
  report.native_info = info;
  if (info < 0 || info > order) {
    return checked::ProviderDefect(info, report);
  }
  internal_lapack_layout::Unpack(packed, rhs);
  if (info > 0) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}

}  // namespace

Result<LapackWorkspacePlan> QueryGtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalView<float> matrix, DenseBlasMatrixView<float> rhs) {
  return Query(provider, matrix, rhs);
}
Status Gtsv(const ReferenceLapackProvider& provider,
            LapackTridiagonalView<float> matrix, DenseBlasMatrixView<float> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, matrix, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalView<double> matrix, DenseBlasMatrixView<double> rhs) {
  return Query(provider, matrix, rhs);
}
Status Gtsv(const ReferenceLapackProvider& provider,
            LapackTridiagonalView<double> matrix,
            DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, matrix, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, matrix, rhs);
}
Status Gtsv(const ReferenceLapackProvider& provider,
            LapackTridiagonalView<std::complex<float>> matrix,
            DenseBlasMatrixView<std::complex<float>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, matrix, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, matrix, rhs);
}
Status Gtsv(const ReferenceLapackProvider& provider,
            LapackTridiagonalView<std::complex<double>> matrix,
            DenseBlasMatrixView<std::complex<double>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, matrix, rhs, plan, workspace, report);
}

}  // namespace asc
