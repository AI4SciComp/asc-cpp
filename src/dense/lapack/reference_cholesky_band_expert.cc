#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; explicit nonallocating INTEGER lifetimes.
#include <optional>
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
#include "asc/dense/providers/lapack_cholesky_band_expert.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "internal_band_abi.h"
#include "internal_band_driver_layout.h"
#include "internal_band_driver_limits.h"
#include "internal_band_expert.h"
#include "internal_layout.h"

namespace asc {
namespace {
namespace band_internal = internal_band_expert;
namespace driver_layout_internal = internal_band_driver_layout;
namespace layout_internal = internal_lapack_layout;
constexpr auto kScalar = static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr auto kReal = static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
enum class Mode : std::uint8_t { kCompute, kEquilibrate, kSupplied };
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "spbsvx";
  static constexpr auto kCall = LAPACK_spbsvx_base;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dpbsvx";
  static constexpr auto kCall = LAPACK_dpbsvx_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cpbsvx";
  static constexpr auto kCall = LAPACK_cpbsvx_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zpbsvx";
  static constexpr auto kCall = LAPACK_zpbsvx_base;
};

template <typename T, Mode FactorMode>
struct Arguments {
  using Real = DenseBlasRealType<T>;
  using Original =
      std::conditional_t<FactorMode == Mode::kEquilibrate, T, const T>;
  using Factor = std::conditional_t<FactorMode == Mode::kSupplied, const T, T>;
  using Rhs = std::conditional_t<FactorMode == Mode::kCompute, const T, T>;
  using Scale =
      std::conditional_t<FactorMode == Mode::kSupplied, const Real, Real>;
  static constexpr bool kPackOriginal =
      DenseBlasComplex<T> && FactorMode != Mode::kSupplied;
  static constexpr char kFact =
      std::array{'N', 'E', 'F'}[static_cast<std::size_t>(FactorMode)];
  LapackPositiveDefiniteBandView<Original> original;
  LapackPositiveDefiniteBandView<Factor> factors;
  DenseBlasMatrixView<Rhs> rhs;
  DenseBlasMatrixView<T> solution;
  DenseBlasVectorView<Real> forward_error;
  DenseBlasVectorView<Real> backward_error;
  const Real* reciprocal_condition;
  std::optional<DenseBlasVectorView<Scale>> scales;
  const LapackCholeskyEquilibration* actual_equilibration;
  LapackCholeskyEquilibration supplied_equilibration;

  [[nodiscard]] std::array<ConstMemoryView, 9> Operands() const {
    return {original.storage().reachable_storage(),
            factors.storage().reachable_storage(),
            rhs.reachable_storage(),
            solution.reachable_storage(),
            forward_error.reachable_storage(),
            backward_error.reachable_storage(),
            band_internal::ObjectStorage(*reciprocal_condition),
            scales ? scales->reachable_storage()
                   : ConstMemoryView{nullptr, 0, MemorySpace::kHost},
            actual_equilibration == nullptr
                ? ConstMemoryView{nullptr, 0, MemorySpace::kHost}
                : band_internal::ObjectStorage(*actual_equilibration)};
  }
};

template <typename T, Mode FactorMode>
Status CheckShapes(const Arguments<T, FactorMode>& args) {
  const auto n = args.original.order();
  if (args.factors.order() != n ||
      args.factors.bandwidth() != args.original.bandwidth() ||
      args.factors.triangle() != args.original.triangle() ||
      args.rhs.rows() != n || args.solution.rows() != n ||
      args.rhs.columns() != args.solution.columns() ||
      args.forward_error.size() != args.rhs.columns() ||
      args.backward_error.size() != args.rhs.columns() ||
      args.forward_error.increment() != 1 ||
      args.backward_error.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  if constexpr (FactorMode != Mode::kCompute) {
    const bool used =
        FactorMode == Mode::kEquilibrate ||
        args.supplied_equilibration == LapackCholeskyEquilibration::kDiagonal;
    if (!args.scales || args.scales->increment() != 1 ||
        (args.scales->size() != n && (used || args.scales->size() != 0))) {
      return Status(ErrorCode::kShape);
    }
  }
  if constexpr (FactorMode == Mode::kSupplied) {
    if (args.supplied_equilibration != LapackCholeskyEquilibration::kNone &&
        args.supplied_equilibration != LapackCholeskyEquilibration::kDiagonal) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  return Status::Ok();
}

template <typename T, Mode FactorMode>
Status Check(const ReferenceLapackProvider& provider,
             const Arguments<T, FactorMode>& args) {
  auto status = band_internal::CheckOperands(provider, args.Operands());
  if (!status.ok()) {
    return status;
  }
  status = CheckShapes(args);
  if (!status.ok()) {
    return status;
  }
  return internal_band_driver_limits::Check(
      args.original.order(), args.original.bandwidth(),
      driver_layout_internal::LeadingDimension(args.original,
                                               args.kPackOriginal),
      band_internal::LeadingDimension(args.factors), args.rhs.columns(),
      layout_internal::LeadingDimension(args.rhs),
      layout_internal::LeadingDimension(args.solution),
      args.original.triangle(), args.kFact, DenseBlasComplex<T>,
      std::numeric_limits<lapack_int>::max());
}

template <typename T, Mode FactorMode>
Result<LapackPlanIdentity> Identity(const ReferenceLapackProvider& provider,
                                    const Arguments<T, FactorMode>& args) {
  const std::int64_t layouts =
      (args.original.layout() == DenseBlasLayout::kRowMajor ? 1 : 0) |
      (args.factors.layout() == DenseBlasLayout::kRowMajor ? 2 : 0) |
      (args.rhs.layout() == DenseBlasLayout::kRowMajor ? 4 : 0) |
      (args.solution.layout() == DenseBlasLayout::kRowMajor ? 8 : 0);
  const auto mode_flags =
      static_cast<std::int64_t>(FactorMode) |
      (args.original.triangle() == DenseBlasTriangle::kLower ? 4 : 0) |
      (args.supplied_equilibration == LapackCholeskyEquilibration::kDiagonal
           ? 8
           : 0) |
      (layouts << 4);
  return LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{args.original.order(), args.original.bandwidth(),
                 args.rhs.columns(),
                 driver_layout_internal::LeadingDimension(args.original,
                                                          args.kPackOriginal),
                 band_internal::LeadingDimension(args.factors),
                 layout_internal::LeadingDimension(args.rhs),
                 layout_internal::LeadingDimension(args.solution)},
      std::array<std::int64_t, 7>{
          mode_flags, args.original.storage().leading_dimension(),
          args.factors.storage().leading_dimension(),
          args.rhs.leading_dimension(), args.solution.leading_dimension(),
          args.scales ? args.scales->size() : 0,
          args.scales ? args.scales->increment() : 1},
      provider.identity());
}

template <typename T, Mode FactorMode>
Status SetWorkspace(const Arguments<T, FactorMode>& args,
                    LapackWorkspacePlan& plan) {
  const auto n = args.original.order();
  if (n == 0) {
    return Status::Ok();
  }
  using Real = DenseBlasRealType<T>;
  const auto count = (DenseBlasComplex<T> ? 2 : 3) * n;
  plan.regions[kScalar] = {count, count, sizeof(T), alignof(T)};
  if constexpr (DenseBlasComplex<T>) {
    plan.regions[kReal] = {n, n, sizeof(Real), alignof(Real)};
  } else {
    plan.regions[kInteger] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
  }
  auto status = driver_layout_internal::AddBandPacking(
      args.original, args.kPackOriginal, plan);
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

template <typename T, Mode FactorMode>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Arguments<T, FactorMode>& args) {
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
struct CallStorage {
  using Real = DenseBlasRealType<T>;
  using Auxiliary = std::conditional_t<DenseBlasComplex<T>, Real, lapack_int>;
  std::array<T, 5> dummy{};
  std::array<Real, 3> real_dummy{};
  Auxiliary auxiliary_dummy{};
  T* a = nullptr;
  T* af = nullptr;
  T* b = nullptr;
  T* x = nullptr;
  T* work = nullptr;
  Real* scales = nullptr;
  Real* ferr = nullptr;
  Real* berr = nullptr;
  Auxiliary* auxiliary = nullptr;
};

template <typename T, Mode FactorMode>
void Prepare(const Arguments<T, FactorMode>& args,
             const LapackWorkspace& workspace, CallStorage<T>& storage) {
  using Real = DenseBlasRealType<T>;
  using Auxiliary = typename CallStorage<T>::Auxiliary;
  const auto n = args.original.order();
  const bool active = n != 0;
  const bool rhs_active = active && args.rhs.columns() != 0;
  auto* cursor =
      static_cast<T*>(workspace.regions[layout_internal::kRegion].data());
  storage.a = active ? const_cast<T*>(driver_layout_internal::PackOriginal(
                           args.original, cursor, args.kPackOriginal))
                     : &storage.dummy[0];
  if constexpr (FactorMode == Mode::kSupplied) {
    storage.af = active ? const_cast<T*>(band_internal::PackBand(args.factors,
                                                                 cursor, false))
                        : &storage.dummy[1];
  } else {
    storage.af = active
                     ? driver_layout_internal::ReserveBand(args.factors, cursor)
                     : &storage.dummy[1];
  }
  storage.b = rhs_active
                  ? const_cast<T*>(layout_internal::Pack(args.rhs, cursor))
                  : &storage.dummy[2];
  storage.x = rhs_active ? driver_layout_internal::ReserveSolution(
                               args.solution, cursor)
                         : &storage.dummy[3];
  storage.work = active ? static_cast<T*>(workspace.regions[kScalar].data())
                        : &storage.dummy[4];
  constexpr auto kAuxiliary = DenseBlasComplex<T> ? kReal : kInteger;
  storage.auxiliary =
      active ? static_cast<Auxiliary*>(workspace.regions[kAuxiliary].data())
             : &storage.auxiliary_dummy;
  if constexpr (!DenseBlasComplex<T>) {
    for (extent_t i = 0; i < n; ++i) {
      ::new (static_cast<void*>(storage.auxiliary + i)) lapack_int;
    }
  }
  storage.scales = args.scales && args.scales->size() != 0
                       ? const_cast<Real*>(args.scales->data())
                       : &storage.real_dummy[0];
  storage.ferr = args.rhs.columns() != 0 ? args.forward_error.data()
                                         : &storage.real_dummy[1];
  storage.berr = args.rhs.columns() != 0 ? args.backward_error.data()
                                         : &storage.real_dummy[2];
}

template <typename T, Mode FactorMode>
lapack_int Call(const Arguments<T, FactorMode>& args, CallStorage<T>& storage,
                char& equilibration, LapackReport& report) {
  const char fact = args.kFact;
  const char triangle =
      args.original.triangle() == DenseBlasTriangle::kUpper ? 'U' : 'L';
  const auto n = static_cast<lapack_int>(args.original.order());
  const auto kd = static_cast<lapack_int>(args.original.bandwidth());
  const auto nrhs = static_cast<lapack_int>(args.rhs.columns());
  const auto lda =
      static_cast<lapack_int>(driver_layout_internal::LeadingDimension(
          args.original, args.kPackOriginal));
  const auto ldaf =
      static_cast<lapack_int>(band_internal::LeadingDimension(args.factors));
  const auto ldb =
      static_cast<lapack_int>(layout_internal::LeadingDimension(args.rhs));
  const auto ldx =
      static_cast<lapack_int>(layout_internal::LeadingDimension(args.solution));
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native<T>::kCall(&fact, &triangle, &n, &kd, &nrhs, storage.a, &lda,
                   storage.af, &ldaf, &equilibration, storage.scales, storage.b,
                   &ldb, storage.x, &ldx,
                   const_cast<DenseBlasRealType<T>*>(args.reciprocal_condition),
                   storage.ferr, storage.berr, storage.work, storage.auxiliary,
                   &info, 1, 1, 1);
  return info;
}

template <typename T, Mode FactorMode>
bool LegalOutput(const Arguments<T, FactorMode>& args, lapack_int info,
                 char equilibration) {
  const auto n = args.original.order();
  if (info < 0 || info > n + 1 ||
      (equilibration != 'N' && equilibration != 'Y')) {
    return false;
  }
  if constexpr (FactorMode == Mode::kCompute) {
    return equilibration == 'N';
  }
  if constexpr (FactorMode == Mode::kSupplied) {
    const char expected =
        args.supplied_equilibration == LapackCholeskyEquilibration::kDiagonal
            ? 'Y'
            : 'N';
    return equilibration == expected && (info == 0 || info == n + 1);
  }
  return true;
}

template <typename T, Mode FactorMode>
void Publish(const Arguments<T, FactorMode>& args,
             const CallStorage<T>& storage, lapack_int info,
             char equilibration) {
  if constexpr (FactorMode == Mode::kEquilibrate) {
    *const_cast<LapackCholeskyEquilibration*>(args.actual_equilibration) =
        equilibration == 'Y' ? LapackCholeskyEquilibration::kDiagonal
                             : LapackCholeskyEquilibration::kNone;
    if (equilibration == 'Y') {
      driver_layout_internal::PublishOriginal(storage.a, args.original,
                                              args.kPackOriginal);
    }
  }
  if constexpr (FactorMode != Mode::kCompute) {
    if (equilibration == 'Y') {
      layout_internal::Unpack(storage.b, args.rhs);
    }
  }
  if constexpr (FactorMode != Mode::kSupplied) {
    band_internal::UnpackBand(storage.af, args.factors, args.original.order());
  }
  if (info == 0 || info == args.original.order() + 1) {
    layout_internal::Unpack(storage.x, args.solution);
  }
}

template <typename T, Mode FactorMode>
Status CheckSuppliedScales(const Arguments<T, FactorMode>& args) {
  if constexpr (FactorMode == Mode::kSupplied) {
    if (args.supplied_equilibration == LapackCholeskyEquilibration::kDiagonal) {
      if (!args.scales) {
        return Status(ErrorCode::kInvalidArgument);
      }
      for (extent_t i = 0; i < args.original.order(); ++i) {
        const auto scale = args.scales->data()[i];
        if (!std::isfinite(scale) || scale <= 0) {
          return Status(ErrorCode::kInvalidArgument);
        }
      }
    }
  }
  return Status::Ok();
}

template <typename T, Mode FactorMode>
Status CheckQuality(const Arguments<T, FactorMode>& args,
                    LapackReport& report) {
  bool nonfinite = !std::isfinite(*args.reciprocal_condition);
  bool negative = *args.reciprocal_condition < 0;
  for (extent_t i = 0; i < args.rhs.columns(); ++i) {
    const auto ferr = args.forward_error.data()[i];
    const auto berr = args.backward_error.data()[i];
    nonfinite = nonfinite || !std::isfinite(ferr) || !std::isfinite(berr);
    negative = negative || ferr < 0 || berr < 0;
  }
  if (nonfinite || negative) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(negative ? ErrorCode::kProvider : ErrorCode::kNumerical);
  }
  return Status::Ok();
}

template <typename T, Mode FactorMode>
Status Execute(const ReferenceLapackProvider& provider,
               const Arguments<T, FactorMode>& args,
               const LapackWorkspacePlan& plan,
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
  status = CheckSuppliedScales(args);
  if (!status.ok()) {
    return status;
  }
  CallStorage<T> storage;
  Prepare(args, workspace, storage);
  if constexpr (FactorMode != Mode::kSupplied) {
    report.factor_family = LapackFactorFamily::kCholesky;
  }
  char equilibration =
      args.supplied_equilibration == LapackCholeskyEquilibration::kDiagonal
          ? 'Y'
          : 'N';
  const auto info = Call(args, storage, equilibration, report);
  status = band_internal::InterpretInfo(info, args.original.order(),
                                        FactorMode != Mode::kSupplied, report);
  if (!LegalOutput(args, info, equilibration)) {
    report.output_validity = LapackOutputValidity::kUnusable;
    if (info >= 0) {
      report.outcome = LapackOutcome::kPartialResult;
    }
    return Status(ErrorCode::kProvider);
  }
  Publish(args, storage, info, equilibration);
  if (info == 0 || info == args.original.order() + 1) {
    status = CheckQuality(args, report);
    if (!status.ok()) {
      return status;
    }
    if (info != 0) {
      report.outcome = LapackOutcome::kAccuracyWarning;
      report.output_validity = LapackOutputValidity::kDocumentedPartial;
      return Status(ErrorCode::kNumerical);
    }
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryPbsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> original,
    LapackPositiveDefiniteBandView<float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  return Query(provider,
               Arguments<float, Mode::kCompute>{
                   original, factors, rhs, solution, forward_error,
                   backward_error, &reciprocal_condition, std::nullopt, nullptr,
                   LapackCholeskyEquilibration::kNone});
}

Status Pbsvx(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const float> original,
             LapackPositiveDefiniteBandView<float> factors,
             DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Arguments<float, Mode::kCompute>{
                     original, factors, rhs, solution, forward_error,
                     backward_error, &reciprocal_condition, std::nullopt,
                     nullptr, LapackCholeskyEquilibration::kNone},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> original,
    LapackPositiveDefiniteBandView<float> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  return Query(provider,
               Arguments<float, Mode::kEquilibrate>{
                   original, factors, rhs, solution, forward_error,
                   backward_error, &reciprocal_condition, std::optional{scales},
                   &equilibration, LapackCholeskyEquilibration::kNone});
}

Status PbsvxEquilibrated(const ReferenceLapackProvider& provider,
                         LapackPositiveDefiniteBandView<float> original,
                         LapackPositiveDefiniteBandView<float> factors,
                         LapackCholeskyEquilibration& equilibration,
                         DenseBlasVectorView<float> scales,
                         DenseBlasMatrixView<float> rhs,
                         DenseBlasMatrixView<float> solution,
                         DenseBlasVectorView<float> forward_error,
                         DenseBlasVectorView<float> backward_error,
                         float& reciprocal_condition,
                         const LapackWorkspacePlan& plan,
                         const LapackWorkspace& workspace,
                         LapackReport& report) {
  return Execute(
      provider,
      Arguments<float, Mode::kEquilibrate>{
          original, factors, rhs, solution, forward_error, backward_error,
          &reciprocal_condition, std::optional{scales}, &equilibration,
          LapackCholeskyEquilibration::kNone},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> original,
    LapackPositiveDefiniteBandView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  return Query(provider, Arguments<float, Mode::kSupplied>{
                             original, factors, rhs, solution, forward_error,
                             backward_error, &reciprocal_condition,
                             std::optional{scales}, nullptr, equilibration});
}

Status PbsvxFactored(const ReferenceLapackProvider& provider,
                     LapackPositiveDefiniteBandView<const float> original,
                     LapackPositiveDefiniteBandView<const float> factors,
                     LapackCholeskyEquilibration equilibration,
                     DenseBlasVectorView<const float> scales,
                     DenseBlasMatrixView<float> rhs,
                     DenseBlasMatrixView<float> solution,
                     DenseBlasVectorView<float> forward_error,
                     DenseBlasVectorView<float> backward_error,
                     float& reciprocal_condition,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Arguments<float, Mode::kSupplied>{
          original, factors, rhs, solution, forward_error, backward_error,
          &reciprocal_condition, std::optional{scales}, nullptr, equilibration},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  return Query(provider,
               Arguments<double, Mode::kCompute>{
                   original, factors, rhs, solution, forward_error,
                   backward_error, &reciprocal_condition, std::nullopt, nullptr,
                   LapackCholeskyEquilibration::kNone});
}

Status Pbsvx(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const double> original,
             LapackPositiveDefiniteBandView<double> factors,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Arguments<double, Mode::kCompute>{
                     original, factors, rhs, solution, forward_error,
                     backward_error, &reciprocal_condition, std::nullopt,
                     nullptr, LapackCholeskyEquilibration::kNone},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> original,
    LapackPositiveDefiniteBandView<double> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  return Query(provider,
               Arguments<double, Mode::kEquilibrate>{
                   original, factors, rhs, solution, forward_error,
                   backward_error, &reciprocal_condition, std::optional{scales},
                   &equilibration, LapackCholeskyEquilibration::kNone});
}

Status PbsvxEquilibrated(const ReferenceLapackProvider& provider,
                         LapackPositiveDefiniteBandView<double> original,
                         LapackPositiveDefiniteBandView<double> factors,
                         LapackCholeskyEquilibration& equilibration,
                         DenseBlasVectorView<double> scales,
                         DenseBlasMatrixView<double> rhs,
                         DenseBlasMatrixView<double> solution,
                         DenseBlasVectorView<double> forward_error,
                         DenseBlasVectorView<double> backward_error,
                         double& reciprocal_condition,
                         const LapackWorkspacePlan& plan,
                         const LapackWorkspace& workspace,
                         LapackReport& report) {
  return Execute(
      provider,
      Arguments<double, Mode::kEquilibrate>{
          original, factors, rhs, solution, forward_error, backward_error,
          &reciprocal_condition, std::optional{scales}, &equilibration,
          LapackCholeskyEquilibration::kNone},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  return Query(provider, Arguments<double, Mode::kSupplied>{
                             original, factors, rhs, solution, forward_error,
                             backward_error, &reciprocal_condition,
                             std::optional{scales}, nullptr, equilibration});
}

Status PbsvxFactored(const ReferenceLapackProvider& provider,
                     LapackPositiveDefiniteBandView<const double> original,
                     LapackPositiveDefiniteBandView<const double> factors,
                     LapackCholeskyEquilibration equilibration,
                     DenseBlasVectorView<const double> scales,
                     DenseBlasMatrixView<double> rhs,
                     DenseBlasMatrixView<double> solution,
                     DenseBlasVectorView<double> forward_error,
                     DenseBlasVectorView<double> backward_error,
                     double& reciprocal_condition,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Arguments<double, Mode::kSupplied>{
          original, factors, rhs, solution, forward_error, backward_error,
          &reciprocal_condition, std::optional{scales}, nullptr, equilibration},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  return Query(provider,
               Arguments<std::complex<float>, Mode::kCompute>{
                   original, factors, rhs, solution, forward_error,
                   backward_error, &reciprocal_condition, std::nullopt, nullptr,
                   LapackCholeskyEquilibration::kNone});
}

Status Pbsvx(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const std::complex<float>> original,
             LapackPositiveDefiniteBandView<std::complex<float>> factors,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Arguments<std::complex<float>, Mode::kCompute>{
                     original, factors, rhs, solution, forward_error,
                     backward_error, &reciprocal_condition, std::nullopt,
                     nullptr, LapackCholeskyEquilibration::kNone},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> original,
    LapackPositiveDefiniteBandView<std::complex<float>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  return Query(provider,
               Arguments<std::complex<float>, Mode::kEquilibrate>{
                   original, factors, rhs, solution, forward_error,
                   backward_error, &reciprocal_condition, std::optional{scales},
                   &equilibration, LapackCholeskyEquilibration::kNone});
}

Status PbsvxEquilibrated(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> original,
    LapackPositiveDefiniteBandView<std::complex<float>> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Execute(
      provider,
      Arguments<std::complex<float>, Mode::kEquilibrate>{
          original, factors, rhs, solution, forward_error, backward_error,
          &reciprocal_condition, std::optional{scales}, &equilibration,
          LapackCholeskyEquilibration::kNone},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  return Query(provider, Arguments<std::complex<float>, Mode::kSupplied>{
                             original, factors, rhs, solution, forward_error,
                             backward_error, &reciprocal_condition,
                             std::optional{scales}, nullptr, equilibration});
}

Status PbsvxFactored(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Execute(
      provider,
      Arguments<std::complex<float>, Mode::kSupplied>{
          original, factors, rhs, solution, forward_error, backward_error,
          &reciprocal_condition, std::optional{scales}, nullptr, equilibration},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  return Query(provider,
               Arguments<std::complex<double>, Mode::kCompute>{
                   original, factors, rhs, solution, forward_error,
                   backward_error, &reciprocal_condition, std::nullopt, nullptr,
                   LapackCholeskyEquilibration::kNone});
}

Status Pbsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Execute(provider,
                 Arguments<std::complex<double>, Mode::kCompute>{
                     original, factors, rhs, solution, forward_error,
                     backward_error, &reciprocal_condition, std::nullopt,
                     nullptr, LapackCholeskyEquilibration::kNone},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> original,
    LapackPositiveDefiniteBandView<std::complex<double>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  return Query(provider,
               Arguments<std::complex<double>, Mode::kEquilibrate>{
                   original, factors, rhs, solution, forward_error,
                   backward_error, &reciprocal_condition, std::optional{scales},
                   &equilibration, LapackCholeskyEquilibration::kNone});
}

Status PbsvxEquilibrated(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> original,
    LapackPositiveDefiniteBandView<std::complex<double>> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Execute(
      provider,
      Arguments<std::complex<double>, Mode::kEquilibrate>{
          original, factors, rhs, solution, forward_error, backward_error,
          &reciprocal_condition, std::optional{scales}, &equilibration,
          LapackCholeskyEquilibration::kNone},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  return Query(provider, Arguments<std::complex<double>, Mode::kSupplied>{
                             original, factors, rhs, solution, forward_error,
                             backward_error, &reciprocal_condition,
                             std::optional{scales}, nullptr, equilibration});
}

Status PbsvxFactored(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Execute(
      provider,
      Arguments<std::complex<double>, Mode::kSupplied>{
          original, factors, rhs, solution, forward_error, backward_error,
          &reciprocal_condition, std::optional{scales}, nullptr, equilibration},
      plan, workspace, report);
}

}  // namespace asc
