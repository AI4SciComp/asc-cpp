#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "internal_layout.h"
#include "internal_positive_tridiagonal_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace counts = internal_positive_tridiagonal_counts;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr std::string_view kFactor = "spttrf";
  static constexpr std::string_view kSolve = "spttrs";
  static lapack_int Factor(lapack_int n, float* d, float* e) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_spttrf(&n, d, e, &info);
    return info;
  }
  static lapack_int Solve(DenseBlasTriangle triangle, lapack_int n,
                          lapack_int nrhs, const float* d, const float* e,
                          float* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    static_cast<void>(triangle);  // The real native signature has no UPLO.
    LAPACK_spttrs(&n, &nrhs, d, e, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr std::string_view kFactor = "dpttrf";
  static constexpr std::string_view kSolve = "dpttrs";
  static lapack_int Factor(lapack_int n, double* d, double* e) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dpttrf(&n, d, e, &info);
    return info;
  }
  static lapack_int Solve(DenseBlasTriangle triangle, lapack_int n,
                          lapack_int nrhs, const double* d, const double* e,
                          double* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    static_cast<void>(triangle);  // The real native signature has no UPLO.
    LAPACK_dpttrs(&n, &nrhs, d, e, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr std::string_view kFactor = "cpttrf";
  static constexpr std::string_view kSolve = "cpttrs";
  static lapack_int Factor(lapack_int n, float* d, std::complex<float>* e) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cpttrf(&n, d, e, &info);
    return info;
  }
  static lapack_int Solve(DenseBlasTriangle triangle, lapack_int n,
                          lapack_int nrhs, const float* d,
                          const std::complex<float>* e, std::complex<float>* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    const char uplo = triangle == DenseBlasTriangle::kUpper ? 'U' : 'L';
    LAPACK_cpttrs(&uplo, &n, &nrhs, d, e, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr std::string_view kFactor = "zpttrf";
  static constexpr std::string_view kSolve = "zpttrs";
  static lapack_int Factor(lapack_int n, double* d, std::complex<double>* e) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zpttrf(&n, d, e, &info);
    return info;
  }
  static lapack_int Solve(DenseBlasTriangle triangle, lapack_int n,
                          lapack_int nrhs, const double* d,
                          const std::complex<double>* e,
                          std::complex<double>* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    const char uplo = triangle == DenseBlasTriangle::kUpper ? 'U' : 'L';
    LAPACK_zpttrs(&uplo, &n, &nrhs, d, e, b, &ldb, &info);
    return info;
  }
};

template <typename Real, typename T>
Status Metadata(const ReferenceLapackProvider& provider,
                DenseBlasVectorView<Real> d, DenseBlasVectorView<T> e) {
  const auto n = d.size();
  Status status = counts::Factor(n, checked::kLimit);
  if (!status.ok()) {
    return status;
  }
  status = checked::Vector(provider, d, n);
  if (status.ok()) {
    status = checked::Vector(provider, e, n == 0 ? 0 : n - 1);
  }
  if (status.ok()) {
    status = checked::Disjoint(std::array<ConstMemoryView, 2>{
        d.reachable_storage(), e.reachable_storage()});
  }
  return status;
}

template <typename T>
bool Finite(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
  } else {
    return std::isfinite(value);
  }
}

template <typename T>
Status FactorValues(ReferencePositiveDefiniteTridiagonalFactorView<T> factor,
                    LapackReport& report) {
  const auto d = factor.diagonal();
  for (extent_t i = 0; i < d.size(); ++i) {
    if (!std::isfinite(d.data()[i]) || d.data()[i] <= 0) {
      report.outcome = LapackOutcome::kNotPositiveDefinite;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  const auto e = factor.off_diagonal();
  for (extent_t i = 0; i < e.size(); ++i) {
    if (!Finite(e.data()[i])) {
      report.outcome = LapackOutcome::kAccuracyWarning;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}

template <typename T>
Result<LapackWorkspacePlan> QueryFactor(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<T> matrix) {
  Status status = Metadata(provider, matrix.diagonal(), matrix.lower());
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kFactor, checked::Kind<T>(), std::array{matrix.order()},
      std::span<const std::int64_t>{}, provider.identity());
  return key.ok() ? Result<LapackWorkspacePlan>(LapackWorkspacePlan{*key})
                  : Result<LapackWorkspacePlan>(key.status());
}

template <typename T>
Status Factor(const ReferenceLapackProvider& provider,
              LapackPositiveDefiniteTridiagonalView<T> matrix,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  const std::array<ConstMemoryView, 2> operands{
      matrix.diagonal().reachable_storage(),
      matrix.lower().reachable_storage()};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kFactor, report);
  const auto expected = QueryFactor(provider, matrix);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const auto n = matrix.order();
  if (n == 0) {
    return checked::Complete(report);
  }
  T unused{};
  report.called_provider = true;
  const lapack_int info =
      Native<T>::Factor(static_cast<lapack_int>(n), matrix.diagonal().data(),
                        checked::Nonnull(matrix.lower().data(), unused));
  report.native_info = info;
  if (info < 0 || info > n) {
    return checked::ProviderDefect(info, report);
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}

template <typename T>
auto SolveOperands(ReferencePositiveDefiniteTridiagonalFactorView<T> factor,
                   DenseBlasMatrixView<T> rhs) {
  return std::array<ConstMemoryView, 3>{
      factor.diagonal().reachable_storage(),
      factor.off_diagonal().reachable_storage(), rhs.reachable_storage()};
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<T> factor,
    DenseBlasMatrixView<T> rhs) {
  if (factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  const auto n = factor.diagonal().size();
  Status status = Metadata(provider, factor.diagonal(), factor.off_diagonal());
  if (status.ok()) {
    status = checked::Matrix(provider, rhs, n);
  }
  if (status.ok()) {
    status = checked::Disjoint(SolveOperands(factor, rhs));
  }
  if (status.ok()) {
    status =
        counts::Solve(n, rhs.columns(), checked::Leading(rhs), checked::kLimit);
  }
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kSolve, checked::Kind<T>(),
      std::array{n, rhs.columns(), checked::Leading(rhs)},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(factor.triangle()),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (n != 0 && rhs.columns() != 0) {
    status = internal_lapack_layout::AddPacking(rhs, plan);
  }
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             ReferencePositiveDefiniteTridiagonalFactorView<T> factor,
             DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = SolveOperands(factor, rhs);
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kSolve, report);
  const auto expected = QuerySolve(provider, factor, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const auto n = factor.diagonal().size();
  if (n == 0 || rhs.columns() == 0) {
    return checked::Complete(report);
  }
  status = FactorValues(factor, report);
  if (!status.ok()) {
    return status;
  }
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  T* b = internal_lapack_layout::Pack(rhs, cursor);
  const T unused{};
  report.called_provider = true;
  const lapack_int info = Native<T>::Solve(
      factor.triangle(), static_cast<lapack_int>(n),
      static_cast<lapack_int>(rhs.columns()), factor.diagonal().data(),
      checked::Nonnull<const T>(factor.off_diagonal().data(), unused), b,
      static_cast<lapack_int>(checked::Leading(rhs)));
  report.native_info = info;
  if (info != 0) {
    return checked::ProviderDefect(info, report);
  }
  internal_lapack_layout::Unpack(b, rhs);
  for (extent_t j = 0; j < rhs.columns(); ++j) {
    for (extent_t i = 0; i < n; ++i) {
      if (!Finite(b[j * checked::Leading(rhs) + i])) {
        report.outcome = LapackOutcome::kAccuracyWarning;
        report.output_validity = LapackOutputValidity::kDocumentedPartial;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  return checked::Complete(report);
}
}  // namespace

template <DenseBlasScalar Element>
Result<ReferencePositiveDefiniteTridiagonalFactorView<Element>>
ReferencePositiveDefiniteTridiagonalFactorView<Element>::FromRaw(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasVectorView<const DenseBlasRealType<Element>> diagonal,
    DenseBlasVectorView<const Element> off_diagonal) {
  if (triangle != DenseBlasTriangle::kLower &&
      triangle != DenseBlasTriangle::kUpper) {
    return Status(ErrorCode::kInvalidArgument);
  }
  Status status = Metadata(provider, diagonal, off_diagonal);
  if (status.ok()) {
    status = checked::Disjoint(std::array<ConstMemoryView, 3>{
        diagonal.reachable_storage(), off_diagonal.reachable_storage(),
        checked::ObjectStorage(provider)});
  }
  if (!status.ok()) {
    return status;
  }
  return ReferencePositiveDefiniteTridiagonalFactorView(
      diagonal, off_diagonal, triangle, provider.identity());
}

template <DenseBlasScalar Element>
Result<ReferencePositiveDefiniteTridiagonalFactorView<Element>>
ReferencePositiveDefiniteTridiagonalFactorView<Element>::Create(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const Element> factors,
    const LapackReport& report) {
  const auto raw = FromRaw(provider, DenseBlasTriangle::kLower,
                           factors.diagonal(), factors.lower());
  if (!raw.ok()) {
    return raw.status();
  }
  const Status metadata = checked::Disjoint(std::array<ConstMemoryView, 4>{
      checked::ObjectStorage(provider), checked::ObjectStorage(report),
      factors.diagonal().reachable_storage(),
      factors.lower().reachable_storage()});
  if (!metadata.ok()) {
    return metadata;
  }
  const auto name = Native<Element>::kFactor;
  const bool execution =
      factors.order() == 0 ? !report.called_provider && !report.native_info
                           : report.called_provider && report.native_info == 0;
  if (!execution || report.provider != provider.identity() ||
      report.outcome != LapackOutcome::kSuccess ||
      report.output_validity != LapackOutputValidity::kComplete ||
      report.factor_family.has_value() ||
      std::string_view(report.routine.data(), name.size()) != name ||
      report.routine[name.size()] != '\0') {
    return Status(ErrorCode::kInvalidState);
  }
  LapackReport validation;
  const Status status = FactorValues(*raw, validation);
  return status.ok()
             ? raw
             : Result<ReferencePositiveDefiniteTridiagonalFactorView>(status);
}

template <DenseBlasScalar Element>
Result<ReferencePositiveDefiniteTridiagonalFactor<Element>>
ReferencePositiveDefiniteTridiagonalFactor<Element>::CopyFrom(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<Element> factor,
    DenseBlasTriangle triangle, MemoryResource& resource) {
  if (factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  if ((triangle != DenseBlasTriangle::kLower &&
       triangle != DenseBlasTriangle::kUpper) ||
      resource.space() != MemorySpace::kHost) {
    return Status(ErrorCode::kInvalidArgument);
  }
  LapackReport validation;
  const Status status = FactorValues(factor, validation);
  if (!status.ok()) {
    return status;
  }
  const auto n = factor.diagonal().size();
  const auto d_shape = Shape::Create(n);
  const auto e_shape = Shape::Create(n == 0 ? 0 : n - 1);
  if (!d_shape.ok() || !e_shape.ok()) {
    return !d_shape.ok() ? d_shape.status() : e_shape.status();
  }
  auto diagonal = Diagonal::Create(resource, *d_shape);
  if (!diagonal.ok()) {
    return diagonal.status();
  }
  auto off_diagonal = OffDiagonal::Create(resource, *e_shape);
  if (!off_diagonal.ok()) {
    return off_diagonal.status();
  }
  auto d = diagonal->view();
  auto e = off_diagonal->view();
  if (!d.ok() || !e.ok()) {
    return !d.ok() ? d.status() : e.status();
  }
  for (extent_t i = 0; i < n; ++i) {
    d->data()[i] = factor.diagonal().data()[i];
  }
  for (extent_t i = 0; i < factor.off_diagonal().size(); ++i) {
    Element value = factor.off_diagonal().data()[i];
    if constexpr (DenseBlasComplex<Element>) {
      if (triangle != factor.triangle()) {
        value = std::conj(value);
      }
    }
    e->data()[i] = value;
  }
  return ReferencePositiveDefiniteTridiagonalFactor(
      std::move(*diagonal), std::move(*off_diagonal), triangle,
      provider.identity());
}

template <DenseBlasScalar Element>
Result<ReferencePositiveDefiniteTridiagonalFactorView<Element>>
ReferencePositiveDefiniteTridiagonalFactor<Element>::view(
    const ReferenceLapackProvider& provider) const {
  if (provider.identity() != provider_) {
    return Status(ErrorCode::kInvalidState);
  }
  const auto d = diagonal_.view();
  const auto e = off_diagonal_.view();
  if (!d.ok() || !e.ok()) {
    return !d.ok() ? d.status() : e.status();
  }
  const auto diagonal =
      DenseBlasVectorView<const DenseBlasRealType<Element>>::Create(
          d->data(), diagonal_.logical_size(), 1,
          {d->data(),
           static_cast<std::size_t>(diagonal_.logical_size()) *
               sizeof(DenseBlasRealType<Element>),
           MemorySpace::kHost});
  const auto off_diagonal = DenseBlasVectorView<const Element>::Create(
      e->data(), off_diagonal_.logical_size(), 1,
      {e->data(),
       static_cast<std::size_t>(off_diagonal_.logical_size()) * sizeof(Element),
       MemorySpace::kHost});
  if (!diagonal.ok() || !off_diagonal.ok()) {
    return !diagonal.ok() ? diagonal.status() : off_diagonal.status();
  }
  return ReferencePositiveDefiniteTridiagonalFactorView<Element>::FromRaw(
      provider, triangle_, *diagonal, *off_diagonal);
}

template class ReferencePositiveDefiniteTridiagonalFactorView<float>;
Result<LapackWorkspacePlan> QueryPttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<float> matrix) {
  return QueryFactor(provider, matrix);
}
Status Pttrf(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<float> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPttrsWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    DenseBlasMatrixView<float> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Status Pttrs(const ReferenceLapackProvider& provider,
             ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
             DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, factor, rhs, plan, workspace, report);
}

template class ReferencePositiveDefiniteTridiagonalFactorView<double>;
Result<LapackWorkspacePlan> QueryPttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<double> matrix) {
  return QueryFactor(provider, matrix);
}
Status Pttrf(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<double> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPttrsWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    DenseBlasMatrixView<double> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Status Pttrs(const ReferenceLapackProvider& provider,
             ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
             DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, factor, rhs, plan, workspace, report);
}

template class ReferencePositiveDefiniteTridiagonalFactorView<
    std::complex<float>>;
Result<LapackWorkspacePlan> QueryPttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<std::complex<float>> matrix) {
  return QueryFactor(provider, matrix);
}
Status Pttrf(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<std::complex<float>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPttrsWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Status Pttrs(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Solve(provider, factor, rhs, plan, workspace, report);
}

template class ReferencePositiveDefiniteTridiagonalFactorView<
    std::complex<double>>;
Result<LapackWorkspacePlan> QueryPttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<std::complex<double>> matrix) {
  return QueryFactor(provider, matrix);
}
Status Pttrf(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<std::complex<double>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPttrsWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Status Pttrs(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Solve(provider, factor, rhs, plan, workspace, report);
}

template class ReferencePositiveDefiniteTridiagonalFactor<float>;

template class ReferencePositiveDefiniteTridiagonalFactor<double>;

template class ReferencePositiveDefiniteTridiagonalFactor<std::complex<float>>;

template class ReferencePositiveDefiniteTridiagonalFactor<std::complex<double>>;

}  // namespace asc
