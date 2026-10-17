#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

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
#include "asc/dense/providers/lapack_positive_tridiagonal_expert.h"
#include "internal_layout.h"
#include "internal_positive_tridiagonal_expert_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
template <typename T>
using Real = DenseBlasRealType<T>;
template <typename T, typename F>
constexpr bool kSupplied =
    std::is_same_v<F, ReferencePositiveDefiniteTridiagonalFactorView<T>>;
template <typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return "sptsvx";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dptsvx";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "cptsvx";
  } else {
    return "zptsvx";
  }
}
template <typename T, typename F>
auto OffDiagonal(F factor) {
  if constexpr (kSupplied<T, F>) {
    return factor.off_diagonal();
  } else {
    return factor.lower();
  }
}
template <typename T, typename F>
DenseBlasTriangle Triangle(F factor) {
  if constexpr (kSupplied<T, F>) {
    return factor.triangle();
  } else {
    return DenseBlasTriangle::kLower;
  }
}
template <typename T, typename F>
struct Operands {
  LapackPositiveDefiniteTridiagonalView<const T> original;
  F factor;
  DenseBlasMatrixView<const T> rhs;
  DenseBlasMatrixView<T> solution;
  const Real<T>* condition;
  DenseBlasVectorView<Real<T>> ferr;
  DenseBlasVectorView<Real<T>> berr;
  [[nodiscard]] auto Spans() const {
    return std::array<ConstMemoryView, 9>{
        original.diagonal().reachable_storage(),
        original.lower().reachable_storage(),
        factor.diagonal().reachable_storage(),
        OffDiagonal<T>(factor).reachable_storage(),
        rhs.reachable_storage(),
        solution.reachable_storage(),
        {condition, sizeof(Real<T>), MemorySpace::kHost},
        ferr.reachable_storage(),
        berr.reachable_storage()};
  }
};
template <typename T, typename F>
Status Metadata(const ReferenceLapackProvider& provider,
                const Operands<T, F>& a) {
  if constexpr (kSupplied<T, F>) {
    if (a.factor.provider() != provider.identity()) {
      return Status(ErrorCode::kInvalidState);
    }
  }
  const auto n = a.original.order();
  if (a.solution.rows() != n || a.rhs.columns() != a.solution.columns()) {
    return Status(ErrorCode::kShape);
  }
  for (const auto& status : std::array{
           checked::Vector(provider, a.original.diagonal(), n),
           checked::Vector(provider, a.original.lower(), n == 0 ? 0 : n - 1),
           checked::Vector(provider, a.factor.diagonal(), n),
           checked::Vector(provider, OffDiagonal<T>(a.factor),
                           n == 0 ? 0 : n - 1),
           checked::Matrix(provider, a.rhs, n),
           checked::Access(provider, a.solution.reachable_storage()),
           checked::Vector(provider, a.ferr, a.rhs.columns()),
           checked::Vector(provider, a.berr, a.rhs.columns()),
           checked::Disjoint(a.Spans())}) {
    if (!status.ok()) {
      return status;
    }
  }
  return internal_positive_tridiagonal_expert_counts::Solve(
      n, a.rhs.columns(), checked::Leading(a.rhs), DenseBlasComplex<T>,
      checked::kLimit);
}
template <typename T, typename F>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Operands<T, F>& a) {
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
      std::array<std::int64_t, 4>{
          kSupplied<T, F>, static_cast<std::int64_t>(Triangle<T>(a.factor)),
          static_cast<std::int64_t>(a.rhs.layout()),
          static_cast<std::int64_t>(a.solution.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (n == 0) {
    return plan;
  }
  if (!DenseBlasComplex<T> && n > std::numeric_limits<extent_t>::max() / 2) {
    return Status(ErrorCode::kOverflow);
  }
  const auto native = DenseBlasComplex<T> ? n : 2 * n;
  plan.regions[checked::kScalar] = {native, native, sizeof(T), alignof(T)};
  if constexpr (DenseBlasComplex<T>) {
    plan.regions[checked::kReal] = {n, n, sizeof(Real<T>), alignof(Real<T>)};
  }
  plan.regions[checked::kScratch] = {nrhs, nrhs, 2 * sizeof(Real<T>),
                                     alignof(Real<T>)};
  if (nrhs > std::numeric_limits<extent_t>::max() / n) {
    return Status(ErrorCode::kOverflow);
  }
  auto staged = n * nrhs;
  if constexpr (DenseBlasComplex<T> && kSupplied<T, F>) {
    if (Triangle<T>(a.factor) == DenseBlasTriangle::kUpper) {
      if (n - 1 > std::numeric_limits<extent_t>::max() - staged) {
        return Status(ErrorCode::kOverflow);
      }
      staged += n - 1;
    }
  }
  plan.regions[checked::kLayout] = {staged, staged, sizeof(T), alignof(T)};
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
template <typename T, typename F>
Status FactorValues(const Operands<T, F>& a, LapackReport& report) {
  if constexpr (kSupplied<T, F>) {
    for (extent_t i = 0; i < a.original.order(); ++i) {
      const auto d = a.factor.diagonal().data()[i];
      if (!std::isfinite(d) || d <= 0) {
        report.outcome = LapackOutcome::kNotPositiveDefinite;
        report.diagnostic_index = i;
        return Status(ErrorCode::kNumerical);
      }
      if (i + 1 < a.original.order() &&
          !Finite(OffDiagonal<T>(a.factor).data()[i])) {
        report.outcome = LapackOutcome::kAccuracyWarning;
        report.diagnostic_index = i;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  return Status::Ok();
}
template <typename T>
struct Frame {
  T unused{};
  Real<T> unused_real{};
  const T* rhs = nullptr;
  const T* lower = nullptr;
  T* solution = nullptr;
  Real<T>* ferr = nullptr;
  Real<T>* berr = nullptr;
  Real<T> condition = -1;
};
template <typename T, typename F>
void Prepare(const Operands<T, F>& a, const LapackWorkspace& workspace,
             Frame<T>& frame) {
  const auto n = a.original.order();
  const auto nrhs = a.rhs.columns();
  frame.rhs = &frame.unused;
  frame.solution = &frame.unused;
  frame.ferr = &frame.unused_real;
  frame.berr = &frame.unused_real;
  frame.lower = OffDiagonal<T>(a.factor).data();
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  if (nrhs > 0) {
    frame.solution = cursor;
    for (extent_t i = 0; i < n * nrhs; ++i) {
      cursor[i] = T{std::numeric_limits<Real<T>>::quiet_NaN()};
    }
    cursor += n * nrhs;
    frame.rhs = internal_lapack_layout::Pack(a.rhs, cursor);
    frame.ferr =
        static_cast<Real<T>*>(workspace.regions[checked::kScratch].data());
    frame.berr = frame.ferr + nrhs;
    for (extent_t j = 0; j < nrhs; ++j) {
      frame.ferr[j] = -1;
      frame.berr[j] = -1;
    }
  }
  if constexpr (DenseBlasComplex<T> && kSupplied<T, F>) {
    if (Triangle<T>(a.factor) == DenseBlasTriangle::kUpper && n > 1) {
      frame.lower = cursor;
      for (extent_t i = 0; i + 1 < n; ++i) {
        cursor[i] = std::conj(OffDiagonal<T>(a.factor).data()[i]);
      }
    }
  }
}
template <typename T, typename F>
lapack_int Native(const Operands<T, F>& a, const LapackWorkspace& workspace,
                  Frame<T>& frame) {
  const char fact = kSupplied<T, F> ? 'F' : 'N';
  const auto n = static_cast<lapack_int>(a.original.order());
  const auto nrhs = static_cast<lapack_int>(a.rhs.columns());
  const auto ldb = static_cast<lapack_int>(checked::Leading(a.rhs));
  const auto* d = a.original.diagonal().data();
  const auto* e = n < 2 ? &frame.unused : a.original.lower().data();
  // The pinned typed declaration cannot express FACT-dependent constness.
  // FACT=F reads these factors; FACT=N receives mutable output descriptors.
  auto* df = const_cast<Real<T>*>(a.factor.diagonal().data());
  auto* ef = n < 2 ? &frame.unused : const_cast<T*>(frame.lower);
  auto* work = static_cast<T*>(workspace.regions[checked::kScalar].data());
  auto* real = static_cast<Real<T>*>(workspace.regions[checked::kReal].data());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sptsvx(&fact, &n, &nrhs, d, e, df, ef, frame.rhs, &ldb,
                  frame.solution, &n, &frame.condition, frame.ferr, frame.berr,
                  work, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dptsvx(&fact, &n, &nrhs, d, e, df, ef, frame.rhs, &ldb,
                  frame.solution, &n, &frame.condition, frame.ferr, frame.berr,
                  work, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cptsvx(&fact, &n, &nrhs, d, e, df, ef, frame.rhs, &ldb,
                  frame.solution, &n, &frame.condition, frame.ferr, frame.berr,
                  work, real, &info);
  } else {
    LAPACK_zptsvx(&fact, &n, &nrhs, d, e, df, ef, frame.rhs, &ldb,
                  frame.solution, &n, &frame.condition, frame.ferr, frame.berr,
                  work, real, &info);
  }
  return info;
}
template <typename T>
extent_t Offset(DenseBlasMatrixView<T> matrix, extent_t i, extent_t j) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? j * matrix.leading_dimension() + i
             : i * matrix.leading_dimension() + j;
}
template <typename T, typename F>
Status ProtocolFailure(lapack_int info, LapackReport& report) {
  auto status = checked::ProviderDefect(info, report);
  if constexpr (kSupplied<T, F>) {
    report.output_validity = LapackOutputValidity::kUnchanged;
  }
  return status;
}
template <typename T, typename F>
Status Publish(const Operands<T, F>& a, const Frame<T>& frame,
               Real<T>& condition, lapack_int info, LapackReport& report) {
  if (frame.condition < 0) {
    return ProtocolFailure<T, F>(info, report);
  }
  const auto n = a.original.order();
  if (info > 0 && info <= n) {
    // Only FACT=N factors the original matrix. Admitted FACT=F has no
    // documented pivot-failure return in this driver.
    if constexpr (kSupplied<T, F>) {
      return ProtocolFailure<T, F>(info, report);
    }
    if (frame.condition != 0) {
      return ProtocolFailure<T, F>(info, report);
    }
    condition = frame.condition;
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.diagnostic_index = info - 1;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  bool finite = std::isfinite(frame.condition);
  for (extent_t j = 0; j < a.rhs.columns(); ++j) {
    if (frame.ferr[j] < 0 || frame.berr[j] < 0) {
      return ProtocolFailure<T, F>(info, report);
    }
    finite =
        finite && std::isfinite(frame.ferr[j]) && std::isfinite(frame.berr[j]);
  }
  if constexpr (!kSupplied<T, F>) {
    for (extent_t i = 0; i < n; ++i) {
      finite = finite && std::isfinite(a.factor.diagonal().data()[i]);
      if (i + 1 < n) {
        finite = finite && Finite(OffDiagonal<T>(a.factor).data()[i]);
      }
    }
  }
  condition = frame.condition;
  for (extent_t j = 0; j < a.rhs.columns(); ++j) {
    a.ferr.data()[j] = frame.ferr[j];
    a.berr.data()[j] = frame.berr[j];
    for (extent_t i = 0; i < n; ++i) {
      const auto value = frame.solution[j * n + i];
      finite = finite && Finite(value);
      a.solution.data()[Offset(a.solution, i, j)] = value;
    }
  }
  if (!finite || info == n + 1) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = finite ? LapackOutputValidity::kComplete
                                    : LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}
template <typename T, typename F>
Status Execute(const ReferenceLapackProvider& provider, const Operands<T, F>& a,
               Real<T>& condition, const LapackWorkspacePlan& plan,
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
  if (a.original.order() == 0) {
    condition = 1;
    for (extent_t j = 0; j < a.rhs.columns(); ++j) {
      a.ferr.data()[j] = 0;
      a.berr.data()[j] = 0;
    }
    return checked::Complete(report);
  }
  status = FactorValues(a, report);
  if (!status.ok()) {
    return status;
  }
  Frame<T> frame;
  Prepare(a, workspace, frame);
  report.called_provider = true;
  const auto info = Native(a, workspace, frame);
  report.native_info = info;
  if (info < 0 || info > a.original.order() + 1) {
    return ProtocolFailure<T, F>(info, report);
  }
  return Publish(a, frame, condition, info, report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    LapackPositiveDefiniteTridiagonalView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider,
               Operands<float, LapackPositiveDefiniteTridiagonalView<float>>{
                   original, factor, rhs, solution, &reciprocal_condition,
                   forward_error, backward_error});
}
Status Ptsvx(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<const float> original,
             LapackPositiveDefiniteTridiagonalView<float> factor,
             DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution, float& reciprocal_condition,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Operands<float, LapackPositiveDefiniteTridiagonalView<float>>{
                     original, factor, rhs, solution, &reciprocal_condition,
                     forward_error, backward_error},
                 reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(
      provider,
      Operands<float, ReferencePositiveDefiniteTridiagonalFactorView<float>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error});
}
Status Ptsvx(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<const float> original,
             ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
             DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution, float& reciprocal_condition,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Operands<float, ReferencePositiveDefiniteTridiagonalFactorView<float>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error},
      reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    LapackPositiveDefiniteTridiagonalView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider,
               Operands<double, LapackPositiveDefiniteTridiagonalView<double>>{
                   original, factor, rhs, solution, &reciprocal_condition,
                   forward_error, backward_error});
}
Status Ptsvx(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<const double> original,
             LapackPositiveDefiniteTridiagonalView<double> factor,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution, double& reciprocal_condition,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Operands<double, LapackPositiveDefiniteTridiagonalView<double>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error},
      reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(
      provider,
      Operands<double, ReferencePositiveDefiniteTridiagonalFactorView<double>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error});
}
Status Ptsvx(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<const double> original,
             ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution, double& reciprocal_condition,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Operands<double, ReferencePositiveDefiniteTridiagonalFactorView<double>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error},
      reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    LapackPositiveDefiniteTridiagonalView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(
      provider,
      Operands<std::complex<float>,
               LapackPositiveDefiniteTridiagonalView<std::complex<float>>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error});
}
Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    LapackPositiveDefiniteTridiagonalView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Operands<std::complex<float>,
               LapackPositiveDefiniteTridiagonalView<std::complex<float>>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error},
      reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(
      provider,
      Operands<
          std::complex<float>,
          ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error});
}
Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Operands<
          std::complex<float>,
          ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error},
      reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    LapackPositiveDefiniteTridiagonalView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(
      provider,
      Operands<std::complex<double>,
               LapackPositiveDefiniteTridiagonalView<std::complex<double>>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error});
}
Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    LapackPositiveDefiniteTridiagonalView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Operands<std::complex<double>,
               LapackPositiveDefiniteTridiagonalView<std::complex<double>>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error},
      reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(
      provider,
      Operands<
          std::complex<double>,
          ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error});
}
Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Operands<
          std::complex<double>,
          ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>>>{
          original, factor, rhs, solution, &reciprocal_condition, forward_error,
          backward_error},
      reciprocal_condition, plan, workspace, report);
}
}  // namespace asc
