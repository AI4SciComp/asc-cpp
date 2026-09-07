#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating foreign INTEGER lifetimes.
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
#include "asc/dense/providers/lapack_cholesky_band_refinement.h"
#include "internal_band_abi.h"
#include "internal_band_expert.h"
#include "internal_band_refinement_limits.h"
#include "internal_layout.h"

namespace asc {
namespace {
namespace band_internal = internal_band_expert;
namespace layout_internal = internal_lapack_layout;
constexpr auto kScalar = static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr auto kReal = static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "spbrfs";
  static constexpr auto kCall = LAPACK_spbrfs_base;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dpbrfs";
  static constexpr auto kCall = LAPACK_dpbrfs_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cpbrfs";
  static constexpr auto kCall = LAPACK_cpbrfs_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zpbrfs";
  static constexpr auto kCall = LAPACK_zpbrfs_base;
};

template <typename T>
struct Arguments {
  LapackPositiveDefiniteBandView<const T> original;
  LapackPositiveDefiniteBandView<const T> factors;
  DenseBlasMatrixView<const T> rhs;
  DenseBlasMatrixView<T> solution;
  DenseBlasVectorView<DenseBlasRealType<T>> forward_error;
  DenseBlasVectorView<DenseBlasRealType<T>> backward_error;

  [[nodiscard]] std::array<ConstMemoryView, 6> Operands() const {
    return {original.storage().reachable_storage(),
            factors.storage().reachable_storage(),
            rhs.reachable_storage(),
            solution.reachable_storage(),
            forward_error.reachable_storage(),
            backward_error.reachable_storage()};
  }
  [[nodiscard]] bool Active() const {
    return original.order() != 0 && rhs.columns() != 0;
  }
};

template <typename T>
Status Check(const ReferenceLapackProvider& provider,
             const Arguments<T>& args) {
  auto status = band_internal::CheckOperands(provider, args.Operands());
  if (!status.ok()) {
    return status;
  }
  if (args.original.order() != args.factors.order() ||
      args.original.bandwidth() != args.factors.bandwidth() ||
      args.original.triangle() != args.factors.triangle() ||
      args.rhs.rows() != args.original.order() ||
      args.solution.rows() != args.original.order() ||
      args.rhs.columns() != args.solution.columns() ||
      args.forward_error.size() != args.rhs.columns() ||
      args.backward_error.size() != args.rhs.columns() ||
      args.forward_error.increment() != 1 ||
      args.backward_error.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  return internal_band_refinement_limits::Check(
      args.original.order(), args.original.bandwidth(),
      band_internal::LeadingDimension(args.original),
      band_internal::LeadingDimension(args.factors), args.rhs.columns(),
      layout_internal::LeadingDimension(args.rhs),
      layout_internal::LeadingDimension(args.solution),
      args.original.triangle(), std::numeric_limits<lapack_int>::max());
}

template <typename T>
Result<LapackPlanIdentity> Identity(const ReferenceLapackProvider& provider,
                                    const Arguments<T>& args) {
  // Four independent layout bits share one option slot; original ASC strides
  // are options, not foreign INTEGER arguments after explicit row packing.
  const std::int64_t layouts =
      (args.original.layout() == DenseBlasLayout::kRowMajor ? 1 : 0) |
      (args.factors.layout() == DenseBlasLayout::kRowMajor ? 2 : 0) |
      (args.rhs.layout() == DenseBlasLayout::kRowMajor ? 4 : 0) |
      (args.solution.layout() == DenseBlasLayout::kRowMajor ? 8 : 0);
  return LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{args.original.order(), args.original.bandwidth(),
                 args.rhs.columns(),
                 band_internal::LeadingDimension(args.original),
                 band_internal::LeadingDimension(args.factors),
                 layout_internal::LeadingDimension(args.rhs),
                 layout_internal::LeadingDimension(args.solution)},
      std::array<std::int64_t, 6>{
          static_cast<std::int64_t>(args.original.triangle()), layouts,
          args.original.storage().leading_dimension(),
          args.factors.storage().leading_dimension(),
          args.rhs.leading_dimension(), args.solution.leading_dimension()},
      provider.identity());
}

template <typename T>
Status SetWorkspace(const Arguments<T>& args, LapackWorkspacePlan& plan) {
  if (!args.Active()) {
    return Status::Ok();
  }
  using Real = DenseBlasRealType<T>;
  const auto n = args.original.order();
  const auto count = (DenseBlasComplex<T> ? 2 : 3) * n;
  plan.regions[kScalar] = {count, count, sizeof(T), alignof(T)};
  if constexpr (DenseBlasComplex<T>) {
    plan.regions[kReal] = {n, n, sizeof(Real), alignof(Real)};
  } else {
    plan.regions[kInteger] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
  }
  auto status = band_internal::AddBandPacking(args.original, plan);
  if (!status.ok()) {
    return status;
  }
  status = band_internal::AddBandPacking(args.factors, plan);
  if (!status.ok()) {
    return status;
  }
  status = layout_internal::AddPacking(args.rhs, plan);
  if (!status.ok()) {
    return status;
  }
  status = layout_internal::AddPacking(args.solution, plan);
  if (!status.ok()) {
    return status;
  }
  return layout_internal::CheckTotal(plan);
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Arguments<T>& args) {
  auto status = Check(provider, args);
  if (!status.ok()) {
    return status;
  }
  const auto identity = Identity(provider, args);
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  status = SetWorkspace(args, plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}

template <typename T>
lapack_int Call(const Arguments<T>& args, const LapackWorkspace& workspace,
                LapackReport& report) {
  using Real = DenseBlasRealType<T>;
  using Auxiliary = std::conditional_t<DenseBlasComplex<T>, Real, lapack_int>;
  constexpr auto kAuxiliary = DenseBlasComplex<T> ? kReal : kInteger;
  T dummy_a{};
  T dummy_af{};
  T dummy_b{};
  T dummy_x{};
  T dummy_work{};
  Real dummy_ferr{};
  Real dummy_berr{};
  Auxiliary dummy_auxiliary{};
  auto* cursor =
      static_cast<T*>(workspace.regions[layout_internal::kRegion].data());
  const bool active = args.Active();
  const T* a =
      active ? band_internal::PackBand(args.original, cursor, true) : &dummy_a;
  const T* af =
      active ? band_internal::PackBand(args.factors, cursor, false) : &dummy_af;
  const T* b = active ? layout_internal::Pack(args.rhs, cursor) : &dummy_b;
  T* x = active ? layout_internal::Pack(args.solution, cursor) : &dummy_x;
  T* work =
      active ? static_cast<T*>(workspace.regions[kScalar].data()) : &dummy_work;
  auto* auxiliary =
      active ? static_cast<Auxiliary*>(workspace.regions[kAuxiliary].data())
             : &dummy_auxiliary;
  if constexpr (!DenseBlasComplex<T>) {
    if (active) {
      for (extent_t i = 0; i < args.original.order(); ++i) {
        ::new (static_cast<void*>(auxiliary + i)) lapack_int;
      }
    }
  }
  auto* ferr =
      args.rhs.columns() != 0 ? args.forward_error.data() : &dummy_ferr;
  auto* berr =
      args.rhs.columns() != 0 ? args.backward_error.data() : &dummy_berr;
  const char triangle =
      args.original.triangle() == DenseBlasTriangle::kUpper ? 'U' : 'L';
  const auto n = static_cast<lapack_int>(args.original.order());
  const auto kd = static_cast<lapack_int>(args.original.bandwidth());
  const auto nrhs = static_cast<lapack_int>(args.rhs.columns());
  const auto lda =
      static_cast<lapack_int>(band_internal::LeadingDimension(args.original));
  const auto ldaf =
      static_cast<lapack_int>(band_internal::LeadingDimension(args.factors));
  const auto ldb =
      static_cast<lapack_int>(layout_internal::LeadingDimension(args.rhs));
  const auto ldx =
      static_cast<lapack_int>(layout_internal::LeadingDimension(args.solution));
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native<T>::kCall(&triangle, &n, &kd, &nrhs, a, &lda, af, &ldaf, b, &ldb, x,
                   &ldx, ferr, berr, work, auxiliary, &info, 1);
  if (info == 0 && active) {
    layout_internal::Unpack(x, args.solution);
  }
  return info;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               const Arguments<T>& args, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  auto status = band_internal::CheckMetadata(provider, plan, workspace, report,
                                             args.Operands());
  if (!status.ok()) {
    return status;
  }
  band_internal::StartReport(provider, Native<T>::kName, report);
  const auto expected = Query(provider, args);
  if (!expected.ok()) {
    return expected.status();
  }
  status = band_internal::ValidatePlan(provider, *expected, plan, workspace,
                                       args.Operands());
  if (!status.ok()) {
    return status;
  }
  const auto info = Call(args, workspace, report);
  status =
      band_internal::InterpretInfo(info, args.original.order(), false, report);
  if (info == 0) {
    for (extent_t i = 0; i < args.rhs.columns(); ++i) {
      const auto ferr = args.forward_error.data()[i];
      const auto berr = args.backward_error.data()[i];
      if (!std::isfinite(ferr) || ferr < 0 || !std::isfinite(berr) ||
          berr < 0) {
        report.outcome = LapackOutcome::kAccuracyWarning;
        report.output_validity = LapackOutputValidity::kDocumentedPartial;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryPbrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> original,
    LapackPositiveDefiniteBandView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider, Arguments<float>{original, factors, rhs, solution,
                                          forward_error, backward_error});
}
Status Pbrfs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const float> original,
             LapackPositiveDefiniteBandView<const float> factors,
             DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Arguments<float>{original, factors, rhs, solution,
                                  forward_error, backward_error},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider, Arguments<double>{original, factors, rhs, solution,
                                           forward_error, backward_error});
}
Status Pbrfs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const double> original,
             LapackPositiveDefiniteBandView<const double> factors,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Arguments<double>{original, factors, rhs, solution,
                                   forward_error, backward_error},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider,
               Arguments<std::complex<float>>{original, factors, rhs, solution,
                                              forward_error, backward_error});
}
Status Pbrfs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const std::complex<float>> original,
             LapackPositiveDefiniteBandView<const std::complex<float>> factors,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Arguments<std::complex<float>>{original, factors, rhs, solution,
                                     forward_error, backward_error},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider,
               Arguments<std::complex<double>>{original, factors, rhs, solution,
                                               forward_error, backward_error});
}
Status Pbrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Arguments<std::complex<double>>{original, factors, rhs, solution,
                                      forward_error, backward_error},
      plan, workspace, report);
}

}  // namespace asc
