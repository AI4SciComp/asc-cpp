#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
#include <string_view>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_cholesky_packed_robust.h"
#include "internal_cholesky_expert.h"
#include "internal_indefinite.h"
#include "internal_packed_cholesky_expert_counts.h"
#include "internal_packed_triangular.h"
#include "internal_robust_ppsvx_kernel.h"

namespace asc {
namespace {
namespace checked = internal_cholesky_expert;
namespace common = internal_indefinite;
namespace storage = internal_packed_triangular;
constexpr std::size_t kErrors =
    static_cast<std::size_t>(LapackWorkspaceKind::kScratch);
enum class Mode : char { kNew = 'N', kEquilibrate = 'E', kSupplied = 'F' };

char EquilibrationCharacter(LapackCholeskyEquilibration equilibration) {
  switch (equilibration) {
    case LapackCholeskyEquilibration::kNone:
      return 'N';
    case LapackCholeskyEquilibration::kDiagonal:
      return 'Y';
  }
  return '?';
}

template <typename T>
DenseBlasVectorView<const T> EmptyVector() {
  return *DenseBlasVectorView<const T>::Create(
      nullptr, 0, 1, {nullptr, 0, MemorySpace::kHost});
}

template <typename T, Mode M>
struct Values {
  using Real = DenseBlasRealType<T>;
  using A = std::conditional_t<M == Mode::kEquilibrate, T, const T>;
  using Af = std::conditional_t<M == Mode::kSupplied, const T, T>;
  using B = std::conditional_t<M == Mode::kNew, const T, T>;
  using Scale = std::conditional_t<M == Mode::kEquilibrate, Real, const Real>;
  static constexpr bool kPackOriginal =
      DenseBlasComplex<T> && M != Mode::kSupplied;

  DenseBlasPackedMatrixView<A> a;
  DenseBlasPackedMatrixView<Af> af;
  DenseBlasVectorView<Scale> s;
  DenseBlasMatrixView<B> b;
  DenseBlasMatrixView<T> x;
  DenseBlasVectorView<Real> ferr;
  DenseBlasVectorView<Real> berr;
  const Real& rcond;
  const LapackCholeskyEquilibration* equilibration_address;
  LapackCholeskyEquilibration equilibration;

  [[nodiscard]] std::array<ConstMemoryView, 9> Spans() const {
    return {a.reachable_storage(),
            af.reachable_storage(),
            s.reachable_storage(),
            b.reachable_storage(),
            x.reachable_storage(),
            ferr.reachable_storage(),
            berr.reachable_storage(),
            common::Object(rcond),
            ConstMemoryView(equilibration_address,
                            equilibration_address == nullptr
                                ? 0
                                : sizeof(*equilibration_address),
                            MemorySpace::kHost)};
  }
};

template <typename T>
struct Algorithm;

template <>
struct Algorithm<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "asc_robust_sppsvx_v1";
};

template <>
struct Algorithm<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "asc_robust_dppsvx_v1";
};

template <>
struct Algorithm<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "asc_robust_cppsvx_v1";
};

template <>
struct Algorithm<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "asc_robust_zppsvx_v1";
};

template <typename T>
extent_t Leading(DenseBlasMatrixView<T> matrix) {
  // The driver validates both native strides even when NRHS=0. In that
  // case no column is referenced; retain original strides only in identity.
  return matrix.columns() == 0 ? std::max<extent_t>(1, matrix.rows())
                               : common::Leading(matrix);
}

template <typename T, Mode M>
Status Validate(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, const Values<T, M>& values) {
  if (!common::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto n = values.a.order();
  if (values.af.order() != n || values.b.rows() != n || values.x.rows() != n ||
      values.x.columns() != values.b.columns()) {
    return Status(ErrorCode::kShape);
  }
  for (const auto operand : values.Spans()) {
    Status status = common::Accessible(provider, operand);
    if (!status.ok()) {
      return status;
    }
  }
  for (const auto vector : {values.ferr, values.berr}) {
    Status status = checked::Vector(provider, vector, values.b.columns());
    if (!status.ok()) {
      return status;
    }
  }
  if constexpr (M != Mode::kNew) {
    if (M == Mode::kSupplied &&
        EquilibrationCharacter(values.equilibration) == '?') {
      return Status(ErrorCode::kInvalidArgument);
    }
    const bool used =
        M == Mode::kEquilibrate ||
        values.equilibration == LapackCholeskyEquilibration::kDiagonal;
    if (!used && values.s.size() != 0 && values.s.size() != n) {
      return Status(ErrorCode::kShape);
    }
    Status status =
        checked::Vector(provider, values.s, used ? n : values.s.size());
    if (!status.ok()) {
      return status;
    }
  }
  Status status = common::Disjoint(values.Spans());
  if (!status.ok()) {
    return status;
  }
  return internal_packed_cholesky_expert_counts::Driver(
      n, values.b.columns(), n == 0 ? 1 : Leading(values.b),
      n == 0 ? 1 : Leading(values.x), common::kIntegerLimit);
}

template <typename T, Mode M>
Status Workspace(const Values<T, M>& values, LapackWorkspacePlan& plan) {
  const auto n = values.a.order();
  const auto nrhs = values.b.columns();
  if (n == 0) {
    return Status::Ok();
  }
  const auto limit = common::kIntegerLimit;
  extent_t count = 0;
  const auto append = [&count](extent_t a, extent_t b) {
    if (a != 0 && b > (limit - count) / a) {
      return false;
    }
    count += a * b;
    return true;
  };
  if (!append(n, n + 1) || !append(2 * n, nrhs) || !append(5, n) ||
      !append(2, nrhs)) {
    return Status(ErrorCode::kOverflow);
  }
  using Value = typename internal_robust_ppsvx::Engine<T>::Value;
  plan.regions[kErrors] = {count, count, sizeof(Value), alignof(Value)};
  return common::Total(plan);
}

template <typename T, Mode M>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  const Values<T, M>& values) {
  Status status = Validate(provider, triangle, values);
  if (!status.ok()) {
    return status;
  }
  const auto n = values.a.order();
  const auto key = LapackPlanIdentity::Create(
      Algorithm<T>::kName, Algorithm<T>::kKind,
      std::array{n, values.b.columns(), n == 0 ? 1 : Leading(values.b),
                 n == 0 ? 1 : Leading(values.x), values.s.size(),
                 values.ferr.size(), values.berr.size()},
      std::array<std::int64_t, 12>{
          static_cast<std::int64_t>(M), static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(values.a.layout()),
          static_cast<std::int64_t>(values.af.layout()),
          static_cast<std::int64_t>(values.b.layout()),
          static_cast<std::int64_t>(values.x.layout()),
          values.b.leading_dimension(), values.x.leading_dimension(),
          values.s.increment(), values.ferr.increment(),
          values.berr.increment(),
          static_cast<std::int64_t>(values.equilibration)},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  status = Workspace(values, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename T, Mode M>
Status ValidateSupplied(const Values<T, M>& values, DenseBlasTriangle triangle,
                        LapackReport& report) {
  if constexpr (M == Mode::kSupplied) {
    if (values.equilibration == LapackCholeskyEquilibration::kDiagonal) {
      for (extent_t i = 0; i < values.s.size(); ++i) {
        if (!std::isfinite(values.s.data()[i]) || values.s.data()[i] <= 0) {
          return Status(ErrorCode::kInvalidArgument);
        }
      }
    }
    for (extent_t i = 0; i < values.af.order(); ++i) {
      const auto offset = storage::Offset(values.af.order(), triangle,
                                          values.af.layout(), i, i);
      if (values.af.data()[offset] == T{}) {
        report.outcome = LapackOutcome::kSingular;
        report.diagnostic_index = i;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  return Status::Ok();
}

template <typename T, Mode M>
Status Empty(const Values<T, M>& values, LapackReport& report) {
  const_cast<DenseBlasRealType<T>&>(values.rcond) = 1;
  for (extent_t i = 0; i < values.ferr.size(); ++i) {
    values.ferr.data()[i] = 0;
    values.berr.data()[i] = 0;
  }
  if constexpr (M == Mode::kEquilibrate) {
    *const_cast<LapackCholeskyEquilibration*>(values.equilibration_address) =
        LapackCholeskyEquilibration::kNone;
  }
  if constexpr (M != Mode::kSupplied) {
    report.factor_family = LapackFactorFamily::kCholesky;
  }
  return common::Complete(report);
}

template <typename T, Mode M>
Status Load(const Values<T, M>& values, DenseBlasTriangle triangle,
            internal_robust_ppsvx::Engine<T>& engine) {
  using Engine = internal_robust_ppsvx::Engine<T>;
  using Value = typename Engine::Value;
  for (extent_t j = 0; j < engine.n; ++j) {
    for (extent_t i = j; i < engine.n; ++i) {
      const auto row = triangle == DenseBlasTriangle::kLower ? i : j;
      const auto column = triangle == DenseBlasTriangle::kLower ? j : i;
      const auto offset =
          storage::Offset(engine.n, triangle, values.a.layout(), row, column);
      auto entry = Value(i == j ? checked::RealDiagonal(values.a.data()[offset])
                                : values.a.data()[offset]);
      engine.a[engine.Offset(i, j)] =
          triangle == DenseBlasTriangle::kLower
              ? entry
              : internal_robust_ppsvx::Conjugate(entry);
      if (!entry.finite()) {
        return Status(ErrorCode::kInvalidArgument);
      }
      if constexpr (M == Mode::kSupplied) {
        const auto factor_offset = storage::Offset(
            engine.n, triangle, values.af.layout(), row, column);
        entry = Value(values.af.data()[factor_offset]);
        engine.af[engine.Offset(i, j)] =
            triangle == DenseBlasTriangle::kLower
                ? entry
                : internal_robust_ppsvx::Conjugate(entry);
        if (!entry.finite()) {
          return Status(ErrorCode::kInvalidArgument);
        }
      }
    }
  }
  if constexpr (M == Mode::kSupplied) {
    engine.scaled =
        values.equilibration == LapackCholeskyEquilibration::kDiagonal;
    for (extent_t i = 0; i < engine.n; ++i) {
      engine.s[i] =
          Value(engine.scaled ? values.s.data()[i] : typename Engine::Real{1});
    }
  }
  return Status::Ok();
}

template <typename T, Mode M>
Status Prepare(const Values<T, M>& values, DenseBlasTriangle triangle,
               internal_robust_ppsvx::Engine<T>& engine, LapackReport& report) {
  Status status = Load(values, triangle, engine);
  if (!status.ok()) {
    return status;
  }
  if constexpr (M == Mode::kEquilibrate) {
    for (extent_t i = 0; i < engine.n; ++i) {
      if (engine.a[engine.Offset(i, i)].real.fraction <= 0) {
        report.outcome = LapackOutcome::kNotPositiveDefinite;
        report.diagnostic_index = i;
        return Status(ErrorCode::kNumerical);
      }
    }
    if (!engine.Equilibrate()) {
      return Status(ErrorCode::kOverflow);
    }
  }
  if constexpr (M != Mode::kSupplied) {
    const auto pivot = engine.Factor();
    if (!engine.valid) {
      return Status(ErrorCode::kOverflow);
    }
    if (pivot != engine.n) {
      report.outcome = LapackOutcome::kNotPositiveDefinite;
      report.diagnostic_index = pivot;
      return Status(ErrorCode::kNumerical);
    }
  }
  using Value = typename internal_robust_ppsvx::Engine<T>::Value;
  for (extent_t j = 0; j < engine.nrhs; ++j) {
    for (extent_t i = 0; i < engine.n; ++i) {
      auto entry = Value(common::Entry(values.b, i, j));
      if (!entry.finite()) {
        return Status(ErrorCode::kInvalidArgument);
      }
      if (engine.scaled) {
        entry = entry * engine.s[i].real;
      }
      engine.b[j * engine.n + i] = entry;
    }
  }
  return Status::Ok();
}

template <typename T, Mode M>
bool Representable(internal_robust_ppsvx::Engine<T>& engine) {
  static_cast<void>(engine.Narrow(engine.rcond));
  if constexpr (M != Mode::kSupplied) {
    for (extent_t i = 0; i < engine.n * (engine.n + 1) / 2; ++i) {
      static_cast<void>(engine.Narrow(engine.af[i]));
    }
  }
  if constexpr (M == Mode::kEquilibrate) {
    for (extent_t i = 0; i < engine.n; ++i) {
      static_cast<void>(engine.Narrow(engine.s[i].real));
    }
    if (engine.scaled) {
      for (extent_t i = 0; i < engine.n * (engine.n + 1) / 2; ++i) {
        static_cast<void>(engine.Narrow(engine.a[i]));
      }
    }
  }
  for (extent_t i = 0; i < engine.n * engine.nrhs; ++i) {
    static_cast<void>(engine.Narrow(engine.x[i]));
    if (engine.scaled) {
      static_cast<void>(engine.Narrow(engine.b[i]));
    }
  }
  for (extent_t j = 0; j < engine.nrhs; ++j) {
    static_cast<void>(engine.Narrow(engine.ferr[j].real));
    static_cast<void>(engine.Narrow(engine.berr[j].real));
  }
  return engine.valid;
}

template <typename T, Mode M>
void PublishMatrices(const Values<T, M>& values, DenseBlasTriangle triangle,
                     internal_robust_ppsvx::Engine<T>& engine) {
  for (extent_t j = 0; j < engine.n; ++j) {
    for (extent_t i = j; i < engine.n; ++i) {
      const auto row = triangle == DenseBlasTriangle::kLower ? i : j;
      const auto column = triangle == DenseBlasTriangle::kLower ? j : i;
      if constexpr (M != Mode::kSupplied) {
        auto factor = engine.af[engine.Offset(i, j)];
        if (triangle == DenseBlasTriangle::kUpper) {
          factor = internal_robust_ppsvx::Conjugate(factor);
        }
        values.af.data()[storage::Offset(engine.n, triangle, values.af.layout(),
                                         row, column)] = engine.Narrow(factor);
      }
      if constexpr (M == Mode::kEquilibrate) {
        if (engine.scaled) {
          auto coefficient = engine.a[engine.Offset(i, j)];
          if (triangle == DenseBlasTriangle::kUpper) {
            coefficient = internal_robust_ppsvx::Conjugate(coefficient);
          }
          values.a.data()[storage::Offset(engine.n, triangle, values.a.layout(),
                                          row, column)] =
              engine.Narrow(coefficient);
        }
      }
    }
  }
  for (extent_t j = 0; j < engine.nrhs; ++j) {
    for (extent_t i = 0; i < engine.n; ++i) {
      common::Entry(values.x, i, j) = engine.Narrow(engine.x[j * engine.n + i]);
      if constexpr (M != Mode::kNew) {
        if (engine.scaled) {
          common::Entry(values.b, i, j) =
              engine.Narrow(engine.b[j * engine.n + i]);
        }
      }
    }
  }
}

template <typename T, Mode M>
Status Publish(const Values<T, M>& values, DenseBlasTriangle triangle,
               internal_robust_ppsvx::Engine<T>& engine, LapackReport& report) {
  using Real = DenseBlasRealType<T>;
  PublishMatrices(values, triangle, engine);
  const Real reciprocal_condition = engine.Narrow(engine.rcond);
  const_cast<Real&>(values.rcond) = reciprocal_condition;
  const bool warning =
      reciprocal_condition < std::numeric_limits<Real>::epsilon() ||
      !engine.accurate;
  for (extent_t j = 0; j < engine.nrhs; ++j) {
    values.ferr.data()[j] = engine.Narrow(engine.ferr[j].real);
    values.berr.data()[j] = engine.Narrow(engine.berr[j].real);
  }
  if constexpr (M == Mode::kEquilibrate) {
    *const_cast<LapackCholeskyEquilibration*>(values.equilibration_address) =
        engine.scaled ? LapackCholeskyEquilibration::kDiagonal
                      : LapackCholeskyEquilibration::kNone;
    for (extent_t i = 0; i < engine.n; ++i) {
      values.s.data()[i] = engine.Narrow(engine.s[i].real);
    }
  }
  if (warning) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  if constexpr (M != Mode::kSupplied) {
    report.factor_family = LapackFactorFamily::kCholesky;
  }
  return common::Complete(report);
}

Status RangeFailure(LapackReport& report) {
  report.outcome = LapackOutcome::kAccuracyWarning;
  return Status(ErrorCode::kOverflow);
}

template <typename T, Mode M>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, const Values<T, M>& values,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  Status status =
      common::Metadata(provider, plan, workspace, report, values.Spans());
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Algorithm<T>::kName, report);
  const auto expected = Query(provider, triangle, values);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, values.Spans());
  if (!status.ok()) {
    return status;
  }
  status = ValidateSupplied(values, triangle, report);
  if (!status.ok()) {
    return status;
  }
  if (values.a.order() == 0) {
    return Empty(values, report);
  }
  using Engine = internal_robust_ppsvx::Engine<T>;
  using Value = typename Engine::Value;
  const auto count =
      static_cast<std::size_t>(expected->regions[kErrors].minimum_entries);
  auto* scratch = new (workspace.regions[kErrors].data()) Value[count];
  Engine engine(values.a.order(), values.b.columns(), scratch);
  status = Prepare(values, triangle, engine, report);
  if (!status.ok()) {
    return status.code() == ErrorCode::kOverflow ? RangeFailure(report)
                                                 : status;
  }
  engine.Condition();
  engine.Solutions();
  if (!Representable<T, M>(engine)) {
    return RangeFailure(report);
  }
  return Publish(values, triangle, engine, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryRobustPpsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  const Values<float, Mode::kNew> values{original,
                                         factors,
                                         EmptyVector<float>(),
                                         rhs,
                                         solution,
                                         forward_error,
                                         backward_error,
                                         reciprocal_condition,
                                         nullptr,
                                         LapackCholeskyEquilibration::kNone};
  return Query(provider, triangle, values);
}

Status RobustPpsvx(const ReferenceLapackProvider& provider,
                   DenseBlasTriangle triangle,
                   DenseBlasPackedMatrixView<const float> original,
                   DenseBlasPackedMatrixView<float> factors,
                   DenseBlasMatrixView<const float> rhs,
                   DenseBlasMatrixView<float> solution,
                   DenseBlasVectorView<float> forward_error,
                   DenseBlasVectorView<float> backward_error,
                   float& reciprocal_condition, const LapackWorkspacePlan& plan,
                   const LapackWorkspace& workspace, LapackReport& report) {
  const Values<float, Mode::kNew> values{original,
                                         factors,
                                         EmptyVector<float>(),
                                         rhs,
                                         solution,
                                         forward_error,
                                         backward_error,
                                         reciprocal_condition,
                                         nullptr,
                                         LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> original,
    DenseBlasPackedMatrixView<float> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  const Values<float, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Query(provider, triangle, values);
}

Status RobustPpsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> original,
    DenseBlasPackedMatrixView<float> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<float, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  const Values<float, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Query(provider, triangle, values);
}

Status RobustPpsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<float, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  const Values<double, Mode::kNew> values{original,
                                          factors,
                                          EmptyVector<double>(),
                                          rhs,
                                          solution,
                                          forward_error,
                                          backward_error,
                                          reciprocal_condition,
                                          nullptr,
                                          LapackCholeskyEquilibration::kNone};
  return Query(provider, triangle, values);
}

Status RobustPpsvx(const ReferenceLapackProvider& provider,
                   DenseBlasTriangle triangle,
                   DenseBlasPackedMatrixView<const double> original,
                   DenseBlasPackedMatrixView<double> factors,
                   DenseBlasMatrixView<const double> rhs,
                   DenseBlasMatrixView<double> solution,
                   DenseBlasVectorView<double> forward_error,
                   DenseBlasVectorView<double> backward_error,
                   double& reciprocal_condition,
                   const LapackWorkspacePlan& plan,
                   const LapackWorkspace& workspace, LapackReport& report) {
  const Values<double, Mode::kNew> values{original,
                                          factors,
                                          EmptyVector<double>(),
                                          rhs,
                                          solution,
                                          forward_error,
                                          backward_error,
                                          reciprocal_condition,
                                          nullptr,
                                          LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> original,
    DenseBlasPackedMatrixView<double> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  const Values<double, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Query(provider, triangle, values);
}

Status RobustPpsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> original,
    DenseBlasPackedMatrixView<double> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<double, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  const Values<double, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Query(provider, triangle, values);
}

Status RobustPpsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<double, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  const Values<std::complex<float>, Mode::kNew> values{
      original,
      factors,
      EmptyVector<float>(),
      rhs,
      solution,
      forward_error,
      backward_error,
      reciprocal_condition,
      nullptr,
      LapackCholeskyEquilibration::kNone};
  return Query(provider, triangle, values);
}

Status RobustPpsvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<std::complex<float>, Mode::kNew> values{
      original,
      factors,
      EmptyVector<float>(),
      rhs,
      solution,
      forward_error,
      backward_error,
      reciprocal_condition,
      nullptr,
      LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> original,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  const Values<std::complex<float>, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Query(provider, triangle, values);
}

Status RobustPpsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> original,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<std::complex<float>, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition) {
  const Values<std::complex<float>, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Query(provider, triangle, values);
}

Status RobustPpsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<std::complex<float>, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  const Values<std::complex<double>, Mode::kNew> values{
      original,
      factors,
      EmptyVector<double>(),
      rhs,
      solution,
      forward_error,
      backward_error,
      reciprocal_condition,
      nullptr,
      LapackCholeskyEquilibration::kNone};
  return Query(provider, triangle, values);
}

Status RobustPpsvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<std::complex<double>, Mode::kNew> values{
      original,
      factors,
      EmptyVector<double>(),
      rhs,
      solution,
      forward_error,
      backward_error,
      reciprocal_condition,
      nullptr,
      LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> original,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  const Values<std::complex<double>, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Query(provider, triangle, values);
}

Status RobustPpsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> original,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<std::complex<double>, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryRobustPpsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition) {
  const Values<std::complex<double>, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Query(provider, triangle, values);
}

Status RobustPpsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const Values<std::complex<double>, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Execute(provider, triangle, values, plan, workspace, report);
}

}  // namespace asc
