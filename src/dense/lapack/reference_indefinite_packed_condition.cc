#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
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
#include "asc/dense/providers/lapack_indefinite_packed_condition.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_expert.h"
#include "internal_indefinite_packed_condition_counts.h"
#include "internal_packed_triangular.h"

namespace asc {
namespace {

namespace bk = internal_indefinite;
namespace expert = internal_indefinite_expert;
namespace packed = internal_packed_triangular;

template <typename T>
std::string_view Name(bool hermitian) {
  if constexpr (std::is_same_v<T, float>) {
    return "sspcon";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dspcon";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "chpcon" : "cspcon";
  } else {
    return hermitian ? "zhpcon" : "zspcon";
  }
}

void Call([[maybe_unused]] bool hermitian, char triangle, lapack_int n,
          const float* a, lapack_int* pivots, float norm, float* condition,
          float* work, lapack_int& info) {
  LAPACK_sspcon(&triangle, &n, a, pivots, &norm, condition, work, pivots + n,
                &info);
}

void Call([[maybe_unused]] bool hermitian, char triangle, lapack_int n,
          const double* a, lapack_int* pivots, double norm, double* condition,
          double* work, lapack_int& info) {
  LAPACK_dspcon(&triangle, &n, a, pivots, &norm, condition, work, pivots + n,
                &info);
}

void Call(bool hermitian, char triangle, lapack_int n,
          const std::complex<float>* a, lapack_int* pivots, float norm,
          float* condition, std::complex<float>* work, lapack_int& info) {
  if (hermitian) {
    LAPACK_chpcon(&triangle, &n, a, pivots, &norm, condition, work, &info);
  } else {
    LAPACK_cspcon(&triangle, &n, a, pivots, &norm, condition, work, &info);
  }
}

void Call(bool hermitian, char triangle, lapack_int n,
          const std::complex<double>* a, lapack_int* pivots, double norm,
          double* condition, std::complex<double>* work, lapack_int& info) {
  if (hermitian) {
    LAPACK_zhpcon(&triangle, &n, a, pivots, &norm, condition, work, &info);
  } else {
    LAPACK_zspcon(&triangle, &n, a, pivots, &norm, condition, work, &info);
  }
}

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const T> factors, RawLapackPivotView pivots,
    DenseBlasRealType<T> original_norm,
    const DenseBlasRealType<T>& reciprocal_condition, bool hermitian) {
  if (!bk::Triangle(triangle) || !std::isfinite(original_norm) ||
      original_norm < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const std::array spans{
      factors.reachable_storage(), pivots.reachable_storage(),
      bk::Object(reciprocal_condition), bk::Object(provider)};
  for (const Status& status :
       {bk::Accessible(provider, factors.reachable_storage()),
        expert::PivotMetadata(provider, pivots, factors.order()),
        bk::Disjoint(spans),
        internal_indefinite_packed_condition_counts::Condition(
            factors.order(), original_norm == 0, bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const bool active = factors.order() != 0 && original_norm != 0;
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(hermitian), bk::ScalarKind<T>(), std::array{factors.order()},
      std::array<std::int64_t, 5>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(hermitian),
          static_cast<std::int64_t>(factors.layout()),
          static_cast<std::int64_t>(pivots.values().size()),
          static_cast<std::int64_t>(active)},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (active) {
    for (const Status& status :
         {expert::EstimatorWork<T>(factors.order(), false, plan),
          packed::Packing(factors, plan)}) {
      if (!status.ok()) {
        return status;
      }
    }
  }
  return plan;
}

template <typename T>
T Entry(DenseBlasPackedMatrixView<const T> factors, DenseBlasTriangle triangle,
        extent_t i, extent_t j) {
  return factors.data()[packed::Offset(factors.order(), triangle,
                                       factors.layout(), i, j)];
}

template <typename T>
T Adjoint(T value, bool hermitian) {
  if constexpr (DenseBlasComplex<T>) {
    return hermitian ? std::conj(value) : value;
  } else {
    return value;
  }
}

template <typename T>
Status BlockDivisors(DenseBlasPackedMatrixView<const T> factors,
                     DenseBlasTriangle triangle, RawLapackPivotView pivots,
                     bool hermitian, LapackReport& report) {
  // CON checks all scalar-zero positive-pivot blocks before invoking TRS.
  // In particular a later zero 1x1 block takes precedence over a 2x2 divisor.
  for (extent_t i = 0; i < factors.order(); ++i) {
    if (pivots.values()[static_cast<std::size_t>(i)] > 0 &&
        Entry(factors, triangle, i, i) == T{}) {
      return Status::Ok();
    }
  }
  for (extent_t i = 0; i < factors.order();) {
    const auto first_index = i;
    bool zero = false;
    if (pivots.values()[static_cast<std::size_t>(i)] > 0) {
      const auto diagonal = Entry(factors, triangle, i, i);
      zero = hermitian ? bk::Real(diagonal) == 0 : diagonal == T{};
      ++i;
    } else {
      const T off = triangle == DenseBlasTriangle::kUpper
                        ? Entry(factors, triangle, i, i + 1)
                        : Entry(factors, triangle, i + 1, i);
      zero = off == T{};
      if (!zero) {
        const T first =
            Adjoint(off, hermitian && triangle == DenseBlasTriangle::kLower);
        const T second =
            Adjoint(off, hermitian && triangle == DenseBlasTriangle::kUpper);
        zero = (Entry(factors, triangle, i, i) / first) *
                       (Entry(factors, triangle, i + 1, i + 1) / second) -
                   T{1} ==
               T{};
      }
      i += 2;
    }
    if (zero) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = first_index;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasPackedMatrixView<const T> factors,
               RawLapackPivotView pivots, DenseBlasRealType<T> original_norm,
               DenseBlasRealType<T>& reciprocal_condition,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool hermitian) {
  const std::array operands{factors.reachable_storage(),
                            pivots.reachable_storage(),
                            bk::Object(reciprocal_condition)};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(hermitian), report);
  const auto expected = Query(provider, triangle, factors, pivots,
                              original_norm, reciprocal_condition, hermitian);
  if (!expected.ok()) {
    return expected.status();
  }
  status = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (factors.order() == 0 || original_norm == 0) {
    reciprocal_condition = factors.order() == 0 ? 1 : 0;
    return bk::Complete(report);
  }
  status = bk::Paired(pivots.values(), factors.order(), triangle);
  if (!status.ok()) {
    return status;
  }
  status = BlockDivisors(factors, triangle, pivots, hermitian, report);
  if (!status.ok()) {
    return status;
  }
  auto* native_pivots = expert::PreparePivots(pivots, plan, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const T* native_factors =
      packed::Pack(factors, triangle, DenseBlasDiagonal::kNonUnit, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  DenseBlasRealType<T> native_condition =
      std::numeric_limits<DenseBlasRealType<T>>::quiet_NaN();
  report.called_provider = true;
  Call(hermitian, bk::Uplo(triangle), static_cast<lapack_int>(factors.order()),
       native_factors, native_pivots, original_norm, &native_condition, work,
       info);
  report.native_info = info;
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    if (native_pivots[i] != pivots.values()[i]) {
      return bk::Defect(info, report);
    }
  }
  if (info != 0) {
    return bk::Defect(info, report);
  }
  reciprocal_condition = native_condition;
  return expert::Estimate(reciprocal_condition, report);
}

}  // namespace

Result<LapackWorkspacePlan> QuerySpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status Spcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const float> factors,
             RawLapackPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> factors, RawLapackPivotView pivots,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status Spcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const double> factors,
             RawLapackPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status Spcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status Spcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QueryHpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, true);
}

Status Hpcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, true);
}

Result<LapackWorkspacePlan> QueryHpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, true);
}

Status Hpcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, true);
}

}  // namespace asc
