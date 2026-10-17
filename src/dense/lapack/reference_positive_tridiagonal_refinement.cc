
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

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
#include "asc/dense/providers/lapack_positive_tridiagonal_refinement.h"
#include "internal_layout.h"
#include "internal_positive_tridiagonal_refinement_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
template <typename T>
using Real = DenseBlasRealType<T>;
template <typename T>
using Factor = ReferencePositiveDefiniteTridiagonalFactorView<T>;
template <typename T>
using Original = LapackPositiveDefiniteTridiagonalView<const T>;
template <typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return "sptrfs";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dptrfs";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "cptrfs";
  } else {
    return "zptrfs";
  }
}
template <typename T>
struct Operands {
  Original<T> original;
  Factor<T> factor;
  DenseBlasMatrixView<const T> rhs;
  DenseBlasMatrixView<T> solution;
  DenseBlasVectorView<Real<T>> ferr;
  DenseBlasVectorView<Real<T>> berr;
  [[nodiscard]] auto Spans() const {
    return std::array{original.diagonal().reachable_storage(),
                      original.lower().reachable_storage(),
                      factor.diagonal().reachable_storage(),
                      factor.off_diagonal().reachable_storage(),
                      rhs.reachable_storage(),
                      solution.reachable_storage(),
                      ferr.reachable_storage(),
                      berr.reachable_storage()};
  }
};
template <typename T>
Status Metadata(const ReferenceLapackProvider& provider, const Operands<T>& a) {
  if (a.factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  const auto n = a.original.order();
  if (a.factor.diagonal().size() != n || a.solution.rows() != n ||
      a.rhs.columns() != a.solution.columns()) {
    return Status(ErrorCode::kShape);
  }
  const auto factor =
      Factor<T>::FromRaw(provider, a.factor.triangle(), a.factor.diagonal(),
                         a.factor.off_diagonal());
  if (!factor.ok()) {
    return factor.status();
  }
  for (const auto& status : std::array{
           checked::Vector(provider, a.original.diagonal(), n),
           checked::Vector(provider, a.original.lower(), n == 0 ? 0 : n - 1),
           checked::Matrix(provider, a.rhs, n),
           checked::Access(provider, a.solution.reachable_storage()),
           checked::Vector(provider, a.ferr, a.rhs.columns()),
           checked::Vector(provider, a.berr, a.rhs.columns()),
           checked::Disjoint(a.Spans())}) {
    if (!status.ok()) {
      return status;
    }
  }
  return internal_positive_tridiagonal_refinement_counts::Refine(
      n, a.rhs.columns(), checked::Leading(a.rhs), n == 0 ? 1 : n,
      DenseBlasComplex<T>, checked::kLimit);
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Operands<T>& a) {
  auto status = Metadata(provider, a);
  if (!status.ok()) {
    return status;
  }
  const auto n = a.original.order();
  const auto nrhs = a.rhs.columns();
  const auto identity = LapackPlanIdentity::Create(
      Routine<T>(), checked::Kind<T>(),
      std::array{n, nrhs, a.rhs.leading_dimension(),
                 a.solution.leading_dimension()},
      std::array<std::int64_t, 3>{
          static_cast<std::int64_t>(a.factor.triangle()),
          static_cast<std::int64_t>(a.rhs.layout()),
          static_cast<std::int64_t>(a.solution.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (n == 0 || nrhs == 0) {
    return plan;
  }
  const auto scalar_entries = DenseBlasComplex<T> ? n : 2 * n;
  plan.regions[checked::kScalar] = {scalar_entries, scalar_entries, sizeof(T),
                                    alignof(T)};
  if constexpr (DenseBlasComplex<T>) {
    plan.regions[checked::kReal] = {n, n, sizeof(Real<T>), alignof(Real<T>)};
  }
  // Two independent NRHS-length output arrays, sized per RHS. This is ASC
  // staging, not native LWORK. The caller provides one flat live real array.
  plan.regions[checked::kScratch] = {nrhs, nrhs, 2 * sizeof(Real<T>),
                                     alignof(Real<T>)};
  if (nrhs > std::numeric_limits<extent_t>::max() / n) {
    return Status(ErrorCode::kOverflow);
  }
  const auto staged = n * nrhs;
  plan.regions[checked::kLayout] = {staged, staged, sizeof(T), alignof(T)};
  if constexpr (DenseBlasComplex<T>) {
    if (a.factor.triangle() == DenseBlasTriangle::kUpper) {
      if (n - 1 > std::numeric_limits<extent_t>::max() - staged) {
        return Status(ErrorCode::kOverflow);
      }
      plan.regions[checked::kLayout] = {staged + n - 1, staged + n - 1,
                                        sizeof(T), alignof(T)};
    }
  }
  status = internal_lapack_layout::AddPacking(a.rhs, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
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
Status FactorValues(Factor<T> factor, LapackReport& report) {
  for (extent_t i = 0; i < factor.diagonal().size(); ++i) {
    const auto d = factor.diagonal().data()[i];
    if (!std::isfinite(d) || d <= 0) {
      report.outcome = LapackOutcome::kNotPositiveDefinite;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  for (extent_t i = 0; i < factor.off_diagonal().size(); ++i) {
    if (!Finite(factor.off_diagonal().data()[i])) {
      report.outcome = LapackOutcome::kAccuracyWarning;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}
template <typename T>
extent_t Offset(DenseBlasMatrixView<T> matrix, extent_t i, extent_t j) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? j * matrix.leading_dimension() + i
             : i * matrix.leading_dimension() + j;
}
template <typename T>
lapack_int Native(const Operands<T>& a, const T* off_diagonal, const T* rhs,
                  T* solution, Real<T>* ferr, Real<T>* berr,
                  const LapackWorkspace& workspace) {
  const auto n = static_cast<lapack_int>(a.original.order());
  const auto nrhs = static_cast<lapack_int>(a.rhs.columns());
  const auto ldb = static_cast<lapack_int>(checked::Leading(a.rhs));
  const T dummy{};
  const auto* e = n < 2 ? &dummy : off_diagonal;
  const auto* ef = n < 2 ? &dummy : a.factor.off_diagonal().data();
  const auto* d = a.original.diagonal().data();
  const auto* df = a.factor.diagonal().data();
  auto* work = static_cast<T*>(workspace.regions[checked::kScalar].data());
  auto* real = static_cast<Real<T>*>(workspace.regions[checked::kReal].data());
  const char triangle =
      a.factor.triangle() == DenseBlasTriangle::kLower ? 'L' : 'U';
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sptrfs(&n, &nrhs, d, e, df, ef, rhs, &ldb, solution, &n, ferr, berr,
                  work, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dptrfs(&n, &nrhs, d, e, df, ef, rhs, &ldb, solution, &n, ferr, berr,
                  work, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cptrfs(&triangle, &n, &nrhs, d, e, df, ef, rhs, &ldb, solution, &n,
                  ferr, berr, work, real, &info);
  } else {
    LAPACK_zptrfs(&triangle, &n, &nrhs, d, e, df, ef, rhs, &ldb, solution, &n,
                  ferr, berr, work, real, &info);
  }
  return info;
}
template <typename T>
Status Publish(const Operands<T>& a, const T* solution, const Real<T>* ferr,
               const Real<T>* berr, lapack_int info, LapackReport& report) {
  bool finite = true;
  for (extent_t j = 0; j < a.rhs.columns(); ++j) {
    if (ferr[j] < 0 || berr[j] < 0) {
      auto status = checked::ProviderDefect(info, report);
      report.output_validity = LapackOutputValidity::kUnchanged;
      return status;
    }
    finite = finite && std::isfinite(ferr[j]) && std::isfinite(berr[j]);
  }
  const auto n = a.original.order();
  for (extent_t j = 0; j < a.rhs.columns(); ++j) {
    a.ferr.data()[j] = ferr[j];
    a.berr.data()[j] = berr[j];
    for (extent_t i = 0; i < n; ++i) {
      const auto value = solution[j * n + i];
      finite = finite && Finite(value);
      a.solution.data()[Offset(a.solution, i, j)] = value;
    }
  }
  if (!finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider, const Operands<T>& a,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto spans = a.Spans();
  auto status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<T>(), report);
  const auto expected = Query(provider, a);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  const auto n = a.original.order();
  const auto nrhs = a.rhs.columns();
  if (n == 0 || nrhs == 0) {
    for (extent_t j = 0; j < nrhs; ++j) {
      a.ferr.data()[j] = 0;
      a.berr.data()[j] = 0;
    }
    return checked::Complete(report);
  }
  status = FactorValues(a.factor, report);
  if (!status.ok()) {
    return status;
  }
  auto* solution = static_cast<T*>(workspace.regions[checked::kLayout].data());
  for (extent_t j = 0; j < nrhs; ++j) {
    for (extent_t i = 0; i < n; ++i) {
      solution[j * n + i] = a.solution.data()[Offset(a.solution, i, j)];
    }
  }
  auto* cursor = solution + n * nrhs;
  const auto* rhs = internal_lapack_layout::Pack(a.rhs, cursor);
  const T* off_diagonal = a.original.lower().data();
  if constexpr (DenseBlasComplex<T>) {
    if (a.factor.triangle() == DenseBlasTriangle::kUpper) {
      off_diagonal = cursor;
      for (extent_t i = 0; i + 1 < n; ++i) {
        cursor[i] = std::conj(a.original.lower().data()[i]);
      }
    }
  }
  auto* ferr =
      static_cast<Real<T>*>(workspace.regions[checked::kScratch].data());
  auto* berr = ferr + nrhs;
  for (extent_t j = 0; j < nrhs; ++j) {
    ferr[j] = -1;
    berr[j] = -1;
  }
  report.called_provider = true;
  const auto info =
      Native(a, off_diagonal, rhs, solution, ferr, berr, workspace);
  report.native_info = info;
  if (info != 0) {
    status = checked::ProviderDefect(info, report);
    report.output_validity = LapackOutputValidity::kUnchanged;
    return status;
  }
  return Publish(a, solution, ferr, berr, info, report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryPtrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider, Operands<float>{original, factor, rhs, solution,
                                         forward_error, backward_error});
}
Status Ptrfs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<const float> original,
             ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
             DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Operands<float>{original, factor, rhs, solution, forward_error,
                                 backward_error},
                 plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider, Operands<double>{original, factor, rhs, solution,
                                          forward_error, backward_error});
}
Status Ptrfs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<const double> original,
             ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Operands<double>{original, factor, rhs, solution,
                                  forward_error, backward_error},
                 plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider,
               Operands<std::complex<float>>{original, factor, rhs, solution,
                                             forward_error, backward_error});
}
Status Ptrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Operands<std::complex<float>>{original, factor, rhs, solution,
                                               forward_error, backward_error},
                 plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider,
               Operands<std::complex<double>>{original, factor, rhs, solution,
                                              forward_error, backward_error});
}
Status Ptrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Operands<std::complex<double>>{original, factor, rhs, solution,
                                                forward_error, backward_error},
                 plan, workspace, report);
}
}  // namespace asc
