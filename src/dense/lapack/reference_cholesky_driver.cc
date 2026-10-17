#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
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
#include "internal_cholesky_expert.h"
#include "internal_cholesky_expert_counts.h"

namespace asc {
namespace {
namespace checked = internal_cholesky_expert;
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

  DenseBlasMatrixView<A> a;
  DenseBlasMatrixView<Af> af;
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
            checked::ObjectStorage(rcond),
            ConstMemoryView(equilibration_address,
                            equilibration_address == nullptr
                                ? 0
                                : sizeof(*equilibration_address),
                            MemorySpace::kHost)};
  }
};

template <typename T>
struct PackedValues {
  T* a;
  T* af;
  T* b;
  T* x;
  std::array<lapack_int, 4> ld;
};

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sposvx";
  static lapack_int Execute(char mode, char triangle, lapack_int n,
                            lapack_int nrhs, const PackedValues<float>& values,
                            char& equilibration, float* scales, float& rcond,
                            float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work =
        static_cast<float*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<float>(n, workspace);
    LAPACK_sposvx(&mode, &triangle, &n, &nrhs, values.a, values.ld.data(),
                  values.af, &values.ld[1], &equilibration, scales, values.b,
                  &values.ld[2], values.x, &values.ld[3], &rcond, ferr, berr,
                  work, extra, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dposvx";
  static lapack_int Execute(char mode, char triangle, lapack_int n,
                            lapack_int nrhs, const PackedValues<double>& values,
                            char& equilibration, double* scales, double& rcond,
                            double* ferr, double* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work =
        static_cast<double*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<double>(n, workspace);
    LAPACK_dposvx(&mode, &triangle, &n, &nrhs, values.a, values.ld.data(),
                  values.af, &values.ld[1], &equilibration, scales, values.b,
                  &values.ld[2], values.x, &values.ld[3], &rcond, ferr, berr,
                  work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cposvx";
  static lapack_int Execute(char mode, char triangle, lapack_int n,
                            lapack_int nrhs,
                            const PackedValues<std::complex<float>>& values,
                            char& equilibration, float* scales, float& rcond,
                            float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<std::complex<float>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra = static_cast<float*>(workspace.regions[checked::kReal].data());
    LAPACK_cposvx(&mode, &triangle, &n, &nrhs, values.a, values.ld.data(),
                  values.af, &values.ld[1], &equilibration, scales, values.b,
                  &values.ld[2], values.x, &values.ld[3], &rcond, ferr, berr,
                  work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zposvx";
  static lapack_int Execute(char mode, char triangle, lapack_int n,
                            lapack_int nrhs,
                            const PackedValues<std::complex<double>>& values,
                            char& equilibration, double* scales, double& rcond,
                            double* ferr, double* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<std::complex<double>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra =
        static_cast<double*>(workspace.regions[checked::kReal].data());
    LAPACK_zposvx(&mode, &triangle, &n, &nrhs, values.a, values.ld.data(),
                  values.af, &values.ld[1], &equilibration, scales, values.b,
                  &values.ld[2], values.x, &values.ld[3], &rcond, ferr, berr,
                  work, extra, &info);
    return info;
  }
};

template <typename T, Mode M>
Status Validate(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, const Values<T, M>& values) {
  Status status = checked::Triangle(triangle);
  if (!status.ok()) {
    return status;
  }
  status = checked::Matrix(provider, values.a, Values<T, M>::kPackOriginal);
  if (!status.ok()) {
    return status;
  }
  for (const auto& matrix :
       {static_cast<DenseBlasMatrixView<const T>>(values.af),
        static_cast<DenseBlasMatrixView<const T>>(values.b),
        static_cast<DenseBlasMatrixView<const T>>(values.x)}) {
    status = checked::Matrix(provider, matrix);
    if (!status.ok()) {
      return status;
    }
  }
  const auto n = values.a.rows();
  if (values.a.columns() != n || values.af.rows() != n ||
      values.af.columns() != n || values.b.rows() != n ||
      values.x.rows() != n || values.x.columns() != values.b.columns()) {
    return Status(ErrorCode::kShape);
  }
  for (const auto vector : {values.ferr, values.berr}) {
    status = checked::Vector(provider, vector, values.b.columns());
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
    status = checked::Vector(provider, values.s, used ? n : values.s.size());
    if (!status.ok()) {
      return status;
    }
  }
  status = internal_cholesky_expert_counts::System(
      n, values.b.columns(), checked::kIntegerLimit, true);
  return status.ok() ? checked::Disjoint(values.Spans()) : status;
}

template <typename T, Mode M>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  const Values<T, M>& values) {
  Status status = Validate(provider, triangle, values);
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kKind,
      std::array{
          values.a.rows(), values.a.columns(),
          checked::Leading(values.a, Values<T, M>::kPackOriginal),
          values.af.rows(), values.af.columns(), checked::Leading(values.af),
          values.b.rows(), values.b.columns(), checked::Leading(values.b),
          values.x.rows(), values.x.columns(), checked::Leading(values.x),
          values.ferr.size(), values.berr.size(), values.s.size()},
      std::array<std::int64_t, 14>{
          static_cast<std::int64_t>(M), static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(values.a.layout()),
          static_cast<std::int64_t>(values.af.layout()),
          static_cast<std::int64_t>(values.b.layout()),
          static_cast<std::int64_t>(values.x.layout()),
          values.a.leading_dimension(), values.af.leading_dimension(),
          values.b.leading_dimension(), values.x.leading_dimension(),
          values.ferr.increment(), values.berr.increment(),
          values.s.increment(),
          static_cast<std::int64_t>(values.equilibration)},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  status = checked::AddEstimatorWork<T>(values.a.rows(), plan);
  if (!status.ok()) {
    return status;
  }
  if (values.a.rows() != 0) {
    status = checked::AddPacking(values.a, plan, Values<T, M>::kPackOriginal);
    if (!status.ok()) {
      return status;
    }
    for (const auto& matrix :
         {static_cast<DenseBlasMatrixView<const T>>(values.af),
          static_cast<DenseBlasMatrixView<const T>>(values.b),
          static_cast<DenseBlasMatrixView<const T>>(values.x)}) {
      status = checked::AddPacking(matrix, plan);
      if (!status.ok()) {
        return status;
      }
    }
  }
  return plan;
}

template <typename T, Mode M>
Status ValidateSupplied(const Values<T, M>& values, LapackReport& report) {
  if constexpr (M == Mode::kSupplied) {
    if (values.equilibration == LapackCholeskyEquilibration::kDiagonal) {
      for (extent_t i = 0; i < values.s.size(); ++i) {
        if (!std::isfinite(values.s.data()[i]) || values.s.data()[i] <= 0) {
          return Status(ErrorCode::kInvalidArgument);
        }
      }
    }
    return checked::ZeroFactor(values.af, report);
  }
  return Status::Ok();
}

template <typename T, Mode M>
PackedValues<T> Pack(const Values<T, M>& values, DenseBlasTriangle triangle,
                     const LapackWorkspace& workspace) {
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  // The pinned POSVX branches do not mutate A except FACT=E, AF except
  // FACT=N/E, or B except EQUED=Y. These casts accommodate its uniform ABI;
  // the public mode types retain those source-proven const guarantees.
  auto* a = const_cast<T*>(checked::PackTriangle(
      values.a, triangle, true, cursor, Values<T, M>::kPackOriginal));
  T* af;
  if constexpr (M == Mode::kSupplied) {
    af = const_cast<T*>(
        checked::PackTriangle(values.af, triangle, false, cursor));
  } else {
    af = checked::PackOutput(values.af, cursor);
  }
  auto* b = const_cast<T*>(checked::PackFull(values.b, cursor));
  auto* x = checked::PackOutput(values.x, cursor);
  return {a,
          af,
          b,
          x,
          {static_cast<lapack_int>(
               checked::Leading(values.a, Values<T, M>::kPackOriginal)),
           static_cast<lapack_int>(checked::Leading(values.af)),
           static_cast<lapack_int>(checked::Leading(values.b)),
           static_cast<lapack_int>(checked::Leading(values.x))}};
}

template <typename T, Mode M>
void Publish(const Values<T, M>& values, const PackedValues<T>& packed,
             DenseBlasTriangle triangle, char equilibration, bool failed) {
  if constexpr (M == Mode::kEquilibrate) {
    *const_cast<LapackCholeskyEquilibration*>(values.equilibration_address) =
        equilibration == 'Y' ? LapackCholeskyEquilibration::kDiagonal
                             : LapackCholeskyEquilibration::kNone;
    if (equilibration == 'Y') {
      checked::PublishTriangle(packed.a, values.a, triangle, false,
                               Values<T, M>::kPackOriginal);
    }
  }
  if constexpr (M != Mode::kNew) {
    if (equilibration == 'Y') {
      checked::PublishFull(packed.b, values.b);
    }
  }
  if constexpr (M != Mode::kSupplied) {
    checked::PublishTriangle(packed.af, values.af, triangle, failed);
  }
  if (!failed) {
    checked::PublishFull(packed.x, values.x);
  }
}

template <typename T, Mode M>
Status Diagnostics(const Values<T, M>& values, lapack_int info,
                   LapackReport& report) {
  bool finite = std::isfinite(values.rcond);
  if (values.rcond < 0) {
    return checked::ProviderDefect(info, report);
  }
  for (extent_t i = 0; i < values.ferr.size(); ++i) {
    const auto ferr = values.ferr.data()[i];
    const auto berr = values.berr.data()[i];
    if (ferr < 0 || berr < 0) {
      return checked::ProviderDefect(info, report);
    }
    finite = finite && std::isfinite(ferr) && std::isfinite(berr);
  }
  if (info != 0 || !finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}

template <typename T, Mode M>
Status Finish(const Values<T, M>& values, const PackedValues<T>& packed,
              DenseBlasTriangle triangle, char equilibration, lapack_int info,
              LapackReport& report) {
  report.native_info = info;
  const auto n = values.a.rows();
  if (info < 0 || info > n + 1 ||
      (M == Mode::kSupplied && info > 0 && info <= n) ||
      (equilibration != 'N' && equilibration != 'Y') ||
      (M == Mode::kNew && equilibration != 'N') ||
      (M == Mode::kSupplied &&
       equilibration != EquilibrationCharacter(values.equilibration))) {
    return checked::ProviderDefect(info, report);
  }
  const bool failed = info > 0 && info <= n;
  Publish(values, packed, triangle, equilibration, failed);
  if (failed) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  return Diagnostics(values, info, report);
}

template <typename T, Mode M>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, const Values<T, M>& values,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, values.Spans());
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kName, report);
  if constexpr (M != Mode::kSupplied) {
    report.factor_family = LapackFactorFamily::kCholesky;
  }
  const auto expected = Query(provider, triangle, values);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace,
                                 values.Spans());
  if (!status.ok()) {
    return status;
  }
  status = ValidateSupplied(values, report);
  if (!status.ok()) {
    return status;
  }
  if (values.a.rows() == 0) {
    const_cast<DenseBlasRealType<T>&>(values.rcond) = 1;
    for (extent_t i = 0; i < values.ferr.size(); ++i) {
      values.ferr.data()[i] = 0;
      values.berr.data()[i] = 0;
    }
    if constexpr (M == Mode::kEquilibrate) {
      *const_cast<LapackCholeskyEquilibration*>(values.equilibration_address) =
          LapackCholeskyEquilibration::kNone;
    }
    return checked::Complete(report);
  }
  const auto packed = Pack(values, triangle, workspace);
  char equilibration = EquilibrationCharacter(values.equilibration);
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      static_cast<char>(M), checked::TriangleCharacter(triangle),
      static_cast<lapack_int>(values.a.rows()),
      static_cast<lapack_int>(values.b.columns()), packed, equilibration,
      const_cast<DenseBlasRealType<T>*>(values.s.data()),
      const_cast<DenseBlasRealType<T>&>(values.rcond), values.ferr.data(),
      values.berr.data(), workspace);
  return Finish(values, packed, triangle, equilibration, info, report);
}

}  // namespace

Result<LapackWorkspacePlan> QueryPosvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution,
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

Status Posvx(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const float> original,
             DenseBlasMatrixView<float> factors,
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

Result<LapackWorkspacePlan> QueryPosvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> original, DenseBlasMatrixView<float> factors,
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

Status PosvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> original, DenseBlasMatrixView<float> factors,
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

Result<LapackWorkspacePlan> QueryPosvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors,
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

Status PosvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const float> original,
                     DenseBlasMatrixView<const float> factors,
                     LapackCholeskyEquilibration equilibration,
                     DenseBlasVectorView<const float> scales,
                     DenseBlasMatrixView<float> rhs,
                     DenseBlasMatrixView<float> solution,
                     DenseBlasVectorView<float> forward_error,
                     DenseBlasVectorView<float> backward_error,
                     float& reciprocal_condition,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Values<float, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPosvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution,
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

Status Posvx(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const double> original,
             DenseBlasMatrixView<double> factors,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
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

Result<LapackWorkspacePlan> QueryPosvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> original, DenseBlasMatrixView<double> factors,
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

Status PosvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> original, DenseBlasMatrixView<double> factors,
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

Result<LapackWorkspacePlan> QueryPosvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors,
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

Status PosvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const double> original,
                     DenseBlasMatrixView<const double> factors,
                     LapackCholeskyEquilibration equilibration,
                     DenseBlasVectorView<const double> scales,
                     DenseBlasMatrixView<double> rhs,
                     DenseBlasMatrixView<double> solution,
                     DenseBlasVectorView<double> forward_error,
                     DenseBlasVectorView<double> backward_error,
                     double& reciprocal_condition,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Values<double, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPosvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
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

Status Posvx(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<float>> original,
             DenseBlasMatrixView<std::complex<float>> factors,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
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

Result<LapackWorkspacePlan> QueryPosvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
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

Status PosvxEquilibrated(const ReferenceLapackProvider& provider,
                         DenseBlasTriangle triangle,
                         DenseBlasMatrixView<std::complex<float>> original,
                         DenseBlasMatrixView<std::complex<float>> factors,
                         LapackCholeskyEquilibration& equilibration,
                         DenseBlasVectorView<float> scales,
                         DenseBlasMatrixView<std::complex<float>> rhs,
                         DenseBlasMatrixView<std::complex<float>> solution,
                         DenseBlasVectorView<float> forward_error,
                         DenseBlasVectorView<float> backward_error,
                         float& reciprocal_condition,
                         const LapackWorkspacePlan& plan,
                         const LapackWorkspace& workspace,
                         LapackReport& report) {
  const Values<std::complex<float>, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPosvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
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

Status PosvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<float>> original,
                     DenseBlasMatrixView<const std::complex<float>> factors,
                     LapackCholeskyEquilibration equilibration,
                     DenseBlasVectorView<const float> scales,
                     DenseBlasMatrixView<std::complex<float>> rhs,
                     DenseBlasMatrixView<std::complex<float>> solution,
                     DenseBlasVectorView<float> forward_error,
                     DenseBlasVectorView<float> backward_error,
                     float& reciprocal_condition,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Values<std::complex<float>, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPosvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
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

Status Posvx(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<double>> original,
             DenseBlasMatrixView<std::complex<double>> factors,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
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

Result<LapackWorkspacePlan> QueryPosvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
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

Status PosvxEquilibrated(const ReferenceLapackProvider& provider,
                         DenseBlasTriangle triangle,
                         DenseBlasMatrixView<std::complex<double>> original,
                         DenseBlasMatrixView<std::complex<double>> factors,
                         LapackCholeskyEquilibration& equilibration,
                         DenseBlasVectorView<double> scales,
                         DenseBlasMatrixView<std::complex<double>> rhs,
                         DenseBlasMatrixView<std::complex<double>> solution,
                         DenseBlasVectorView<double> forward_error,
                         DenseBlasVectorView<double> backward_error,
                         double& reciprocal_condition,
                         const LapackWorkspacePlan& plan,
                         const LapackWorkspace& workspace,
                         LapackReport& report) {
  const Values<std::complex<double>, Mode::kEquilibrate> values{
      original,       factors,
      scales,         rhs,
      solution,       forward_error,
      backward_error, reciprocal_condition,
      &equilibration, LapackCholeskyEquilibration::kNone};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPosvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
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

Status PosvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<double>> original,
                     DenseBlasMatrixView<const std::complex<double>> factors,
                     LapackCholeskyEquilibration equilibration,
                     DenseBlasVectorView<const double> scales,
                     DenseBlasMatrixView<std::complex<double>> rhs,
                     DenseBlasMatrixView<std::complex<double>> solution,
                     DenseBlasVectorView<double> forward_error,
                     DenseBlasVectorView<double> backward_error,
                     double& reciprocal_condition,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Values<std::complex<double>, Mode::kSupplied> values{
      original, factors,       scales,         rhs,
      solution, forward_error, backward_error, reciprocal_condition,
      nullptr,  equilibration};
  return Execute(provider, triangle, values, plan, workspace, report);
}

}  // namespace asc
