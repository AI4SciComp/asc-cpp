#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <new>  // IWYU pragma: keep; caller-owned provider integer lifetimes.
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_driver.h"
#include "internal_indefinite.h"
#include "internal_indefinite_counts.h"
#include "internal_indefinite_expert.h"
#include "internal_indefinite_expert_counts.h"

namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace expert = internal_indefinite_expert;
namespace counts = internal_indefinite_expert_counts;

template <typename T, bool New>
struct Values {
  using Real = DenseBlasRealType<T>;
  using Af = std::conditional_t<New, T, const T>;
  using Pivot =
      std::conditional_t<New, DenseBlasVectorView<index_t>, RawLapackPivotView>;
  DenseBlasMatrixView<const T> a;
  DenseBlasMatrixView<Af> af;
  Pivot pivots;
  DenseBlasMatrixView<const T> b;
  DenseBlasMatrixView<T> x;
  const Real& rcond;
  DenseBlasVectorView<Real> ferr;
  DenseBlasVectorView<Real> berr;
  bool hermitian;

  [[nodiscard]] auto Spans() const {
    return std::array{a.reachable_storage(),      af.reachable_storage(),
                      pivots.reachable_storage(), b.reachable_storage(),
                      x.reachable_storage(),      bk::Object(rcond),
                      ferr.reachable_storage(),   berr.reachable_storage()};
  }

  [[nodiscard]] extent_t PivotSize() const {
    if constexpr (New) {
      return pivots.size();
    } else {
      return static_cast<extent_t>(pivots.values().size());
    }
  }

  [[nodiscard]] bool PackOriginal() const { return New && hermitian; }
};

template <typename T>
struct PackedValues {
  const T* a;
  T* af;
  const T* b;
  T* x;
  std::array<lapack_int, 4> ld;
  lapack_int* pivots;
  bool hermitian;
};

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view Name([[maybe_unused]] bool hermitian) {
    return "ssysvx";
  }
  static lapack_int Execute(char fact, char triangle, lapack_int n,
                            lapack_int nrhs, const PackedValues<float>& v,
                            float& rcond, float* ferr, float* berr,
                            lapack_int lwork,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<float*>(workspace.regions[bk::kScalar].data());
    auto* extra = v.pivots + n;
    LAPACK_ssysvx(&fact, &triangle, &n, &nrhs, v.a, v.ld.data(), v.af, &v.ld[1],
                  v.pivots, v.b, &v.ld[2], v.x, &v.ld[3], &rcond, ferr, berr,
                  work, &lwork, extra, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view Name([[maybe_unused]] bool hermitian) {
    return "dsysvx";
  }
  static lapack_int Execute(char fact, char triangle, lapack_int n,
                            lapack_int nrhs, const PackedValues<double>& v,
                            double& rcond, double* ferr, double* berr,
                            lapack_int lwork,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<double*>(workspace.regions[bk::kScalar].data());
    auto* extra = v.pivots + n;
    LAPACK_dsysvx(&fact, &triangle, &n, &nrhs, v.a, v.ld.data(), v.af, &v.ld[1],
                  v.pivots, v.b, &v.ld[2], v.x, &v.ld[3], &rcond, ferr, berr,
                  work, &lwork, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view Name(bool hermitian) {
    return hermitian ? "chesvx" : "csysvx";
  }
  static lapack_int Execute(char fact, char triangle, lapack_int n,
                            lapack_int nrhs,
                            const PackedValues<std::complex<float>>& v,
                            float& rcond, float* ferr, float* berr,
                            lapack_int lwork,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<std::complex<float>*>(
        workspace.regions[bk::kScalar].data());
    auto* extra = static_cast<float*>(workspace.regions[expert::kReal].data());
    if (v.hermitian) {
      LAPACK_chesvx(&fact, &triangle, &n, &nrhs, v.a, v.ld.data(), v.af,
                    &v.ld[1], v.pivots, v.b, &v.ld[2], v.x, &v.ld[3], &rcond,
                    ferr, berr, work, &lwork, extra, &info);
    } else {
      LAPACK_csysvx(&fact, &triangle, &n, &nrhs, v.a, v.ld.data(), v.af,
                    &v.ld[1], v.pivots, v.b, &v.ld[2], v.x, &v.ld[3], &rcond,
                    ferr, berr, work, &lwork, extra, &info);
    }
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view Name(bool hermitian) {
    return hermitian ? "zhesvx" : "zsysvx";
  }
  static lapack_int Execute(char fact, char triangle, lapack_int n,
                            lapack_int nrhs,
                            const PackedValues<std::complex<double>>& v,
                            double& rcond, double* ferr, double* berr,
                            lapack_int lwork,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<std::complex<double>*>(
        workspace.regions[bk::kScalar].data());
    auto* extra = static_cast<double*>(workspace.regions[expert::kReal].data());
    if (v.hermitian) {
      LAPACK_zhesvx(&fact, &triangle, &n, &nrhs, v.a, v.ld.data(), v.af,
                    &v.ld[1], v.pivots, v.b, &v.ld[2], v.x, &v.ld[3], &rcond,
                    ferr, berr, work, &lwork, extra, &info);
    } else {
      LAPACK_zsysvx(&fact, &triangle, &n, &nrhs, v.a, v.ld.data(), v.af,
                    &v.ld[1], v.pivots, v.b, &v.ld[2], v.x, &v.ld[3], &rcond,
                    ferr, berr, work, &lwork, extra, &info);
    }
    return info;
  }
};

template <typename T>
extent_t RhsLeading(DenseBlasMatrixView<T> matrix) {
  return matrix.columns() == 0 ? std::max<extent_t>(1, matrix.rows())
                               : bk::Leading(matrix);
}

template <typename T, bool New>
Status Validate(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, const Values<T, New>& v) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto n = v.a.rows();
  if (v.a.columns() != n || v.af.rows() != n || v.af.columns() != n ||
      v.PivotSize() != n || v.b.rows() != n || v.x.rows() != n ||
      v.x.columns() != v.b.columns()) {
    return Status(ErrorCode::kShape);
  }
  for (const Status& status :
       {bk::Matrix(provider, v.a, v.PackOriginal()), bk::Matrix(provider, v.af),
        bk::Matrix(provider, v.b), bk::Matrix(provider, v.x),
        expert::ErrorVector(provider, v.ferr, v.b.columns()),
        expert::ErrorVector(provider, v.berr, v.b.columns()),
        bk::Accessible(provider, v.pivots.reachable_storage()),
        bk::Disjoint(v.Spans()),
        counts::Refinement(n, v.b.columns(), bk::kIntegerLimit),
        internal_indefinite_counts::Solve(n, v.x.columns(), RhsLeading(v.x),
                                          bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if constexpr (New) {
    if (v.pivots.increment() != 1) {
      return Status(ErrorCode::kInvalidArgument);
    }
    Status factor = internal_indefinite_counts::Factor(n, bk::Leading(v.af),
                                                       bk::kIntegerLimit);
    if (!factor.ok()) {
      return factor;
    }
  } else {
    Status pivots = expert::PivotMetadata(provider, v.pivots, n);
    if (!pivots.ok()) {
      return pivots;
    }
  }
  for (const auto span : v.Spans()) {
    if (bk::Overlap(span, bk::Object(provider))) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  return Status::Ok();
}

template <typename T, bool New>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  const Values<T, New>& v) {
  Status status = Validate(provider, triangle, v);
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::Name(v.hermitian), Native<T>::kKind,
      std::array{v.a.rows(), v.a.columns(), bk::Leading(v.a, v.PackOriginal()),
                 v.af.rows(), v.af.columns(), bk::Leading(v.af), v.b.rows(),
                 v.b.columns(), RhsLeading(v.b), v.x.rows(), v.x.columns(),
                 RhsLeading(v.x), v.PivotSize(), v.ferr.size(), v.berr.size()},
      std::array<std::int64_t, 13>{
          static_cast<std::int64_t>(triangle), New, v.hermitian,
          static_cast<std::int64_t>(v.a.layout()), v.a.leading_dimension(),
          static_cast<std::int64_t>(v.af.layout()), v.af.leading_dimension(),
          static_cast<std::int64_t>(v.b.layout()), v.b.leading_dimension(),
          static_cast<std::int64_t>(v.x.layout()), v.x.leading_dimension(),
          v.ferr.increment(), v.berr.increment()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const auto n = v.a.rows();
  if (n == 0) {
    return plan;
  }
  const auto preferred = counts::Expert<DenseBlasRealType<T>>(
      n, DenseBlasComplex<T>, New, bk::kIntegerLimit);
  if (!preferred.ok()) {
    return preferred.status();
  }
  status = expert::EstimatorWork<T>(n, true, plan);
  if (!status.ok()) {
    return status;
  }
  plan.regions[bk::kScalar].preferred_entries = *preferred;
  for (const Status& packing :
       {bk::Packing(v.a, plan, v.PackOriginal()), bk::Packing(v.af, plan),
        bk::Packing(v.b, plan), bk::Packing(v.x, plan)}) {
    if (!packing.ok()) {
      return packing;
    }
  }
  return plan;
}

template <typename T, bool New>
PackedValues<T> Pack(const Values<T, New>& v, DenseBlasTriangle triangle,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace) {
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const auto* a =
      bk::PackTriangle(v.a, triangle, v.hermitian, cursor, v.PackOriginal());
  T* af;
  lapack_int* pivots;
  if constexpr (New) {
    af = expert::PackFull(v.af, cursor, false);
    pivots = ::new (workspace.regions[bk::kPivot].data())
        lapack_int[static_cast<std::size_t>(
            plan.regions[bk::kPivot].minimum_entries)]{};
  } else {
    // The pinned FACT=F branch and its CON/TRS/RFS callees do not assign AF.
    // The shared Fortran signature is mutable solely for FACT=N. No writes
    // through this const-discarding ABI facade occur for supplied factors.
    af = const_cast<T*>(bk::PackTriangle(v.af, triangle, false, cursor));
    pivots = expert::PreparePivots(v.pivots, plan, workspace);
  }
  const auto* b = expert::PackFull(v.b, cursor, true);
  auto* x = expert::PackFull(v.x, cursor, false);
  return {a,
          af,
          b,
          x,
          {static_cast<lapack_int>(bk::Leading(v.a, v.PackOriginal())),
           static_cast<lapack_int>(bk::Leading(v.af)),
           static_cast<lapack_int>(RhsLeading(v.b)),
           static_cast<lapack_int>(RhsLeading(v.x))},
          pivots,
          v.hermitian};
}

template <typename T, bool New>
Status Diagnostics(const Values<T, New>& v, lapack_int info,
                   LapackReport& report) {
  bool finite = std::isfinite(v.rcond);
  if (v.rcond < 0) {
    return bk::Defect(info, report);
  }
  for (extent_t i = 0; i < v.ferr.size(); ++i) {
    const auto forward = v.ferr.data()[i];
    const auto backward = v.berr.data()[i];
    if (forward < 0 || backward < 0) {
      return bk::Defect(info, report);
    }
    finite = finite && std::isfinite(forward) && std::isfinite(backward);
  }
  if (info == v.a.rows() + 1 || !finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return bk::Complete(report);
}

template <typename T, bool New>
Status Finish(const Values<T, New>& v, const PackedValues<T>& packed,
              DenseBlasTriangle triangle, lapack_int info,
              LapackReport& report) {
  const auto n = v.a.rows();
  if (info < 0 || info > n + 1 || (!New && info > 0 && info <= n)) {
    return bk::Defect(info, report);
  }
  if (!bk::Paired(std::span<const lapack_int>(packed.pivots,
                                              static_cast<std::size_t>(n)),
                  n, triangle)
           .ok()) {
    return bk::Defect(info, report);
  }
  const bool factor_failure = info > 0 && info <= n;
  if (factor_failure && v.rcond != 0) {
    return bk::Defect(info, report);
  }
  Status status = factor_failure ? Status(ErrorCode::kNumerical)
                                 : Diagnostics(v, info, report);
  if (status.code() == ErrorCode::kProvider) {
    return status;
  }
  if constexpr (New) {
    for (extent_t i = 0; i < n; ++i) {
      v.pivots.data()[i] = packed.pivots[i];
    }
    bk::PublishTriangle(packed.af, v.af, triangle, false);
  }
  if (factor_failure) {
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    report.outcome = packed.af[(info - 1) * (packed.ld[1] + extent_t{1})] == T{}
                         ? LapackOutcome::kSingular
                         : LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
  } else {
    bk::PublishRhs(packed.x, v.x);
  }
  return status;
}

template <typename T, bool New>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, const Values<T, New>& v,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  Status status = bk::Metadata(provider, plan, workspace, report, v.Spans());
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Native<T>::Name(v.hermitian), report);
  report.factor_family = LapackFactorFamily::kBunchKaufman;
  const auto expected = Query(provider, triangle, v);
  if (!expected.ok()) {
    return expected.status();
  }
  status = bk::Plan(provider, *expected, plan, workspace, v.Spans());
  if (!status.ok()) {
    return status;
  }
  if (v.a.rows() == 0) {
    const_cast<DenseBlasRealType<T>&>(v.rcond) = 1;
    for (extent_t i = 0; i < v.ferr.size(); ++i) {
      v.ferr.data()[i] = 0;
      v.berr.data()[i] = 0;
    }
    return bk::Complete(report);
  }
  if constexpr (!New) {
    status = bk::Paired(v.pivots.values(), v.a.rows(), triangle);
    if (!status.ok()) {
      return status;
    }
    status = expert::BlockDivisors(v.af, v.pivots, triangle, v.hermitian, false,
                                   report);
    if (!status.ok()) {
      return status;
    }
  }
  auto packed = Pack(v, triangle, plan, workspace);
  // Valid local objects satisfy foreign dummy-argument lifetimes even when
  // NRHS=0. The source still factors/estimates but never reads these objects.
  T unused_b{};
  T unused_x{};
  DenseBlasRealType<T> unused_ferr{};
  DenseBlasRealType<T> unused_berr{};
  const bool empty_rhs = v.b.columns() == 0;
  if (empty_rhs) {
    packed.b = &unused_b;
    packed.x = &unused_x;
  }
  const auto count = std::min(
      workspace.regions[bk::kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[bk::kScalar].preferred_entries));
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      New ? 'N' : 'F', bk::Uplo(triangle), static_cast<lapack_int>(v.a.rows()),
      static_cast<lapack_int>(v.b.columns()), packed,
      const_cast<DenseBlasRealType<T>&>(v.rcond),
      empty_rhs ? &unused_ferr : v.ferr.data(),
      empty_rhs ? &unused_berr : v.berr.data(), static_cast<lapack_int>(count),
      workspace);
  report.native_info = info;
  const T expected_work{static_cast<DenseBlasRealType<T>>(
      plan.regions[bk::kScalar].preferred_entries)};
  if (*static_cast<const T*>(workspace.regions[bk::kScalar].data()) !=
      expected_work) {
    return bk::Defect(info, report);
  }
  return Finish(v, packed, triangle, info, report);
}
}  // namespace

Result<LapackWorkspacePlan> QuerySysvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<float, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Query(provider, triangle, values);
}

Status Sysvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Values<float, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySysvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<float, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Query(provider, triangle, values);
}

Status SysvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Values<float, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySysvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<double, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Query(provider, triangle, values);
}

Status Sysvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Values<double, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySysvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<double, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Query(provider, triangle, values);
}

Status SysvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Values<double, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySysvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<std::complex<float>, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Query(provider, triangle, values);
}

Status Sysvx(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<float>> original,
             DenseBlasMatrixView<std::complex<float>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             float& reciprocal_condition,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<float>, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySysvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<std::complex<float>, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Query(provider, triangle, values);
}

Status SysvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<float>> original,
                     DenseBlasMatrixView<const std::complex<float>> factors,
                     RawLapackPivotView pivots,
                     DenseBlasMatrixView<const std::complex<float>> rhs,
                     DenseBlasMatrixView<std::complex<float>> solution,
                     float& reciprocal_condition,
                     DenseBlasVectorView<float> forward_error,
                     DenseBlasVectorView<float> backward_error,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Values<std::complex<float>, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySysvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<std::complex<double>, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Query(provider, triangle, values);
}

Status Sysvx(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<double>> original,
             DenseBlasMatrixView<std::complex<double>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             double& reciprocal_condition,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<double>, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySysvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<std::complex<double>, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Query(provider, triangle, values);
}

Status SysvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<double>> original,
                     DenseBlasMatrixView<const std::complex<double>> factors,
                     RawLapackPivotView pivots,
                     DenseBlasMatrixView<const std::complex<double>> rhs,
                     DenseBlasMatrixView<std::complex<double>> solution,
                     double& reciprocal_condition,
                     DenseBlasVectorView<double> forward_error,
                     DenseBlasVectorView<double> backward_error,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Values<std::complex<double>, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryHesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<std::complex<float>, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, true};
  return Query(provider, triangle, values);
}

Status Hesvx(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<float>> original,
             DenseBlasMatrixView<std::complex<float>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             float& reciprocal_condition,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<float>, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, true};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryHesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<std::complex<float>, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, true};
  return Query(provider, triangle, values);
}

Status HesvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<float>> original,
                     DenseBlasMatrixView<const std::complex<float>> factors,
                     RawLapackPivotView pivots,
                     DenseBlasMatrixView<const std::complex<float>> rhs,
                     DenseBlasMatrixView<std::complex<float>> solution,
                     float& reciprocal_condition,
                     DenseBlasVectorView<float> forward_error,
                     DenseBlasVectorView<float> backward_error,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Values<std::complex<float>, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, true};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryHesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<std::complex<double>, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, true};
  return Query(provider, triangle, values);
}

Status Hesvx(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<double>> original,
             DenseBlasMatrixView<std::complex<double>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             double& reciprocal_condition,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<double>, true> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, true};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryHesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<std::complex<double>, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, true};
  return Query(provider, triangle, values);
}

Status HesvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<double>> original,
                     DenseBlasMatrixView<const std::complex<double>> factors,
                     RawLapackPivotView pivots,
                     DenseBlasMatrixView<const std::complex<double>> rhs,
                     DenseBlasMatrixView<std::complex<double>> solution,
                     double& reciprocal_condition,
                     DenseBlasVectorView<double> forward_error,
                     DenseBlasVectorView<double> backward_error,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Values<std::complex<double>, false> values{
      original,      factors,        pivots,
      rhs,           solution,       reciprocal_condition,
      forward_error, backward_error, true};
  return Execute(provider, triangle, values, plan, workspace, report);
}

}  // namespace asc
