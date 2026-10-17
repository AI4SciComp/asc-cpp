#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed_refinement.h"
#include "internal_indefinite.h"
#include "internal_indefinite_expert.h"
#include "internal_indefinite_packed_refinement_counts.h"
#include "internal_packed_triangular.h"

namespace asc {
namespace {
namespace checked = internal_indefinite;
namespace expert = internal_indefinite_expert;
namespace storage = internal_packed_triangular;

template <typename T>
struct Values {
  DenseBlasPackedMatrixView<const T> a;
  DenseBlasPackedMatrixView<const T> af;
  DenseBlasMatrixView<const T> b;
  DenseBlasMatrixView<T> x;
  DenseBlasVectorView<DenseBlasRealType<T>> ferr;
  DenseBlasVectorView<DenseBlasRealType<T>> berr;

  RawLapackPivotView pivots;
  bool hermitian;

  [[nodiscard]] auto Spans() const {
    return std::array{a.reachable_storage(),     af.reachable_storage(),
                      b.reachable_storage(),     x.reachable_storage(),
                      ferr.reachable_storage(),  berr.reachable_storage(),
                      pivots.reachable_storage()};
  }
};

template <typename T>
struct PackedValues {
  const T* a;
  const T* af;
  const T* b;
  T* x;
  lapack_int ldb;
  lapack_int ldx;
  lapack_int* pivots;
  bool hermitian;
};

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view Name([[maybe_unused]] bool hermitian) {
    return "ssprfs";
  }
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<float>& values, float* ferr,
                            float* berr, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work =
        static_cast<float*>(workspace.regions[checked::kScalar].data());
    auto* extra = values.pivots + n;
    LAPACK_ssprfs(&triangle, &n, &nrhs, values.a, values.af, values.pivots,
                  values.b, &values.ldb, values.x, &values.ldx, ferr, berr,
                  work, extra, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view Name([[maybe_unused]] bool hermitian) {
    return "dsprfs";
  }
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<double>& values, double* ferr,
                            double* berr, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work =
        static_cast<double*>(workspace.regions[checked::kScalar].data());
    auto* extra = values.pivots + n;
    LAPACK_dsprfs(&triangle, &n, &nrhs, values.a, values.af, values.pivots,
                  values.b, &values.ldb, values.x, &values.ldx, ferr, berr,
                  work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view Name(bool hermitian) {
    return hermitian ? "chprfs" : "csprfs";
  }
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<std::complex<float>>& values,
                            float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work = static_cast<std::complex<float>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra = static_cast<float*>(workspace.regions[expert::kReal].data());
    if (values.hermitian) {
      LAPACK_chprfs(&triangle, &n, &nrhs, values.a, values.af, values.pivots,
                    values.b, &values.ldb, values.x, &values.ldx, ferr, berr,
                    work, extra, &info);
    } else {
      LAPACK_csprfs(&triangle, &n, &nrhs, values.a, values.af, values.pivots,
                    values.b, &values.ldb, values.x, &values.ldx, ferr, berr,
                    work, extra, &info);
    }
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view Name(bool hermitian) {
    return hermitian ? "zhprfs" : "zsprfs";
  }
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<std::complex<double>>& values,
                            double* ferr, double* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work = static_cast<std::complex<double>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra = static_cast<double*>(workspace.regions[expert::kReal].data());
    if (values.hermitian) {
      LAPACK_zhprfs(&triangle, &n, &nrhs, values.a, values.af, values.pivots,
                    values.b, &values.ldb, values.x, &values.ldx, ferr, berr,
                    work, extra, &info);
    } else {
      LAPACK_zsprfs(&triangle, &n, &nrhs, values.a, values.af, values.pivots,
                    values.b, &values.ldb, values.x, &values.ldx, ferr, berr,
                    work, extra, &info);
    }
    return info;
  }
};

template <typename T>
Status Validate(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, const Values<T>& values) {
  if (!checked::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  Status status;
  for (const auto& matrix :
       {values.b, static_cast<DenseBlasMatrixView<const T>>(values.x)}) {
    status = checked::Matrix(provider, matrix);
    if (!status.ok()) {
      return status;
    }
  }
  for (const auto& matrix : {values.a, values.af}) {
    status = checked::Accessible(provider, matrix.reachable_storage());
    if (!status.ok()) {
      return status;
    }
  }
  const auto n = values.a.order();
  if (values.af.order() != n || values.b.rows() != n || values.x.rows() != n ||
      values.x.columns() != values.b.columns()) {
    return Status(ErrorCode::kShape);
  }
  for (const auto vector : {values.ferr, values.berr}) {
    status = expert::ErrorVector(provider, vector, values.b.columns());
    if (!status.ok()) {
      return status;
    }
  }
  status = expert::PivotMetadata(provider, values.pivots, n);
  if (!status.ok()) {
    return status;
  }
  for (const auto span : values.Spans()) {
    if (checked::Overlap(span, checked::Object(provider))) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  status = internal_indefinite_packed_refinement_counts::Refine(
      n, values.b.columns(), checked::Leading(values.b),
      checked::Leading(values.x), checked::kIntegerLimit);
  return status.ok() ? checked::Disjoint(values.Spans()) : status;
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  const Values<T>& values) {
  Status status = Validate(provider, triangle, values);
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::Name(values.hermitian), Native<T>::kKind,
      std::array{values.a.order(), values.af.order(), values.b.rows(),
                 values.b.columns(), checked::Leading(values.b),
                 values.x.rows(), values.x.columns(),
                 checked::Leading(values.x), values.ferr.size(),
                 values.berr.size(),
                 static_cast<extent_t>(values.pivots.values().size())},
      std::array<std::int64_t, 10>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(values.a.layout()),
          static_cast<std::int64_t>(values.af.layout()),
          static_cast<std::int64_t>(values.b.layout()),
          static_cast<std::int64_t>(values.x.layout()),
          values.b.leading_dimension(), values.x.leading_dimension(),
          values.ferr.increment(), values.berr.increment(),
          static_cast<std::int64_t>(values.hermitian)},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const bool active = values.a.order() != 0 && values.b.columns() != 0;
  if (!active) {
    return plan;
  }
  status = expert::EstimatorWork<T>(values.a.order(), true, plan);
  if (!status.ok()) {
    return status;
  }
  const auto native_real = DenseBlasComplex<T> ? values.a.order() : 0;
  if (values.b.columns() >
      (std::numeric_limits<extent_t>::max() - native_real) / 2) {
    return Status(ErrorCode::kOverflow);
  }
  const auto real_count = native_real + 2 * values.b.columns();
  plan.regions[expert::kReal] = {real_count, real_count,
                                 sizeof(DenseBlasRealType<T>),
                                 alignof(DenseBlasRealType<T>)};
  for (const auto& matrix : {values.a, values.af}) {
    status = storage::Packing(matrix, plan);
    if (!status.ok()) {
      return status;
    }
  }
  for (const auto& matrix :
       {values.b, static_cast<DenseBlasMatrixView<const T>>(values.x)}) {
    status = checked::Packing(matrix, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

template <typename T>
const T* PackOriginal(DenseBlasPackedMatrixView<const T> a,
                      DenseBlasTriangle triangle, bool hermitian, T*& cursor) {
  if (a.layout() == DenseBlasLayout::kColumnMajor) {
    return a.data();
  }
  T* result = cursor;
  cursor += storage::Entries(a.order());
  for (extent_t j = 0; j < a.order(); ++j) {
    const auto first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto last = triangle == DenseBlasTriangle::kUpper ? j + 1 : a.order();
    for (extent_t i = first; i < last; ++i) {
      const auto source =
          storage::Offset(a.order(), triangle, a.layout(), i, j);
      const auto target = storage::Offset(a.order(), triangle,
                                          DenseBlasLayout::kColumnMajor, i, j);
      if constexpr (DenseBlasComplex<T>) {
        if (hermitian && i == j) {
          result[target] = T{a.data()[source].real(), 0};
          continue;
        }
      }
      result[target] = a.data()[source];
    }
  }
  return result;
}

template <typename T>
PackedValues<T> Pack(const Values<T>& values, DenseBlasTriangle triangle,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace) {
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  const auto* a = PackOriginal(values.a, triangle, values.hermitian, cursor);
  const auto* af =
      storage::Pack(values.af, triangle, DenseBlasDiagonal::kNonUnit, cursor);
  const auto* b = expert::PackFull(values.b, cursor, true);
  auto* x = expert::PackFull(values.x, cursor, true);
  return {a,
          af,
          b,
          x,
          static_cast<lapack_int>(checked::Leading(values.b)),
          static_cast<lapack_int>(checked::Leading(values.x)),
          expert::PreparePivots(values.pivots, plan, workspace),
          values.hermitian};
}

template <typename T>
Status ValidateErrors(const Values<T>& values, LapackReport& report) {
  bool finite = true;
  for (extent_t i = 0; i < values.ferr.size(); ++i) {
    const auto forward = values.ferr.data()[i];
    const auto backward = values.berr.data()[i];
    if (forward < 0 || backward < 0) {
      return checked::Defect(0, report);
    }
    finite = finite && std::isfinite(forward) && std::isfinite(backward);
  }
  if (!finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}

template <typename T>
T Entry(DenseBlasPackedMatrixView<const T> factors, DenseBlasTriangle triangle,
        extent_t i, extent_t j) {
  return factors.data()[storage::Offset(factors.order(), triangle,
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
  for (extent_t i = 0; i < factors.order();) {
    const auto first_index = i;
    bool zero = false;
    if (pivots.values()[static_cast<std::size_t>(i)] > 0) {
      const auto diagonal = Entry(factors, triangle, i, i);
      zero = hermitian ? checked::Real(diagonal) == 0 : diagonal == T{};
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
Status Refine(DenseBlasTriangle triangle, const Values<T>& values,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  const auto packed = Pack(values, triangle, plan, workspace);
  using Real = DenseBlasRealType<T>;
  auto* real_work = static_cast<Real*>(workspace.regions[expert::kReal].data());
  auto* forward = real_work + (DenseBlasComplex<T> ? values.a.order() : 0);
  auto* backward = forward + values.b.columns();
  for (extent_t i = 0; i < values.b.columns(); ++i) {
    forward[i] = std::numeric_limits<Real>::quiet_NaN();
    backward[i] = std::numeric_limits<Real>::quiet_NaN();
  }
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      checked::Uplo(triangle), static_cast<lapack_int>(values.a.order()),
      static_cast<lapack_int>(values.b.columns()), packed, forward, backward,
      workspace);
  report.native_info = info;
  for (std::size_t i = 0; i < values.pivots.values().size(); ++i) {
    if (packed.pivots[i] != values.pivots.values()[i]) {
      return checked::Defect(info, report);
    }
  }
  if (info != 0) {
    return checked::Defect(info, report);
  }
  for (extent_t i = 0; i < values.b.columns(); ++i) {
    values.ferr.data()[i] = forward[i];
    values.berr.data()[i] = backward[i];
  }
  Status status = ValidateErrors(values, report);
  if (status.code() != ErrorCode::kProvider) {
    checked::PublishRhs(packed.x, values.x);
  }
  return status;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, const Values<T>& values,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  Status status =
      checked::Metadata(provider, plan, workspace, report, values.Spans());
  if (!status.ok()) {
    return status;
  }
  checked::Start(provider, Native<T>::Name(values.hermitian), report);
  const auto expected = Query(provider, triangle, values);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::Plan(provider, *expected, plan, workspace, values.Spans());
  if (!status.ok()) {
    return status;
  }
  if (values.a.order() == 0 || values.b.columns() == 0) {
    for (extent_t i = 0; i < values.ferr.size(); ++i) {
      values.ferr.data()[i] = 0;
      values.berr.data()[i] = 0;
    }
    return checked::Complete(report);
  }
  status = checked::Paired(values.pivots.values(), values.a.order(), triangle);
  if (!status.ok()) {
    return status;
  }
  status = BlockDivisors(values.af, triangle, values.pivots, values.hermitian,
                         report);
  if (!status.ok()) {
    return status;
  }
  return Refine(triangle, values, plan, workspace, report);
}
}  // namespace

Result<LapackWorkspacePlan> QuerySprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<float> values{original,      factors,        rhs,    solution,
                             forward_error, backward_error, pivots, false};
  return Query(provider, triangle, values);
}

Status Sprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const float> original,
             DenseBlasPackedMatrixView<const float> factors,
             RawLapackPivotView pivots, DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<float> values{original,      factors,        rhs,    solution,
                             forward_error, backward_error, pivots, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<double> values{original,      factors,        rhs,    solution,
                              forward_error, backward_error, pivots, false};
  return Query(provider, triangle, values);
}

Status Sprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const double> original,
             DenseBlasPackedMatrixView<const double> factors,
             RawLapackPivotView pivots, DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<double> values{original,      factors,        rhs,    solution,
                              forward_error, backward_error, pivots, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<std::complex<float>> values{
      original,      factors,        rhs,    solution,
      forward_error, backward_error, pivots, false};
  return Query(provider, triangle, values);
}

Status Sprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> original,
             DenseBlasPackedMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<float>> values{
      original,      factors,        rhs,    solution,
      forward_error, backward_error, pivots, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QuerySprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<std::complex<double>> values{
      original,      factors,        rhs,    solution,
      forward_error, backward_error, pivots, false};
  return Query(provider, triangle, values);
}

Status Sprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> original,
             DenseBlasPackedMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<double>> values{
      original,      factors,        rhs,    solution,
      forward_error, backward_error, pivots, false};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryHprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<std::complex<float>> values{
      original,      factors,        rhs,    solution,
      forward_error, backward_error, pivots, true};
  return Query(provider, triangle, values);
}

Status Hprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> original,
             DenseBlasPackedMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<float>> values{
      original,      factors,        rhs,    solution,
      forward_error, backward_error, pivots, true};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryHprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<std::complex<double>> values{
      original,      factors,        rhs,    solution,
      forward_error, backward_error, pivots, true};
  return Query(provider, triangle, values);
}

Status Hprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> original,
             DenseBlasPackedMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<double>> values{
      original,      factors,        rhs,    solution,
      forward_error, backward_error, pivots, true};
  return Execute(provider, triangle, values, plan, workspace, report);
}

}  // namespace asc
