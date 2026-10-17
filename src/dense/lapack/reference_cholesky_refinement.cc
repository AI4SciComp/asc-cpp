#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_refinement.h"
#include "internal_cholesky_expert.h"
#include "internal_cholesky_expert_counts.h"

namespace asc {
namespace {
namespace checked = internal_cholesky_expert;

template <typename T>
struct Values {
  DenseBlasMatrixView<const T> a;
  DenseBlasMatrixView<const T> af;
  DenseBlasMatrixView<const T> b;
  DenseBlasMatrixView<T> x;
  DenseBlasVectorView<DenseBlasRealType<T>> ferr;
  DenseBlasVectorView<DenseBlasRealType<T>> berr;

  [[nodiscard]] auto Spans() const {
    return std::array{a.reachable_storage(),    af.reachable_storage(),
                      b.reachable_storage(),    x.reachable_storage(),
                      ferr.reachable_storage(), berr.reachable_storage()};
  }
};

template <typename T>
struct PackedValues {
  const T* a;
  const T* af;
  const T* b;
  T* x;
  lapack_int lda;
  lapack_int ldaf;
  lapack_int ldb;
  lapack_int ldx;
};

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sporfs";
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<float>& values, float* ferr,
                            float* berr, const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work =
        static_cast<float*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<float>(n, workspace);
    LAPACK_sporfs(&triangle, &n, &nrhs, values.a, &values.lda, values.af,
                  &values.ldaf, values.b, &values.ldb, values.x, &values.ldx,
                  ferr, berr, work, extra, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dporfs";
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<double>& values, double* ferr,
                            double* berr, const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work =
        static_cast<double*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<double>(n, workspace);
    LAPACK_dporfs(&triangle, &n, &nrhs, values.a, &values.lda, values.af,
                  &values.ldaf, values.b, &values.ldb, values.x, &values.ldx,
                  ferr, berr, work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cporfs";
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<std::complex<float>>& values,
                            float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<std::complex<float>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra = static_cast<float*>(workspace.regions[checked::kReal].data());
    LAPACK_cporfs(&triangle, &n, &nrhs, values.a, &values.lda, values.af,
                  &values.ldaf, values.b, &values.ldb, values.x, &values.ldx,
                  ferr, berr, work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zporfs";
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<std::complex<double>>& values,
                            double* ferr, double* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<std::complex<double>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra =
        static_cast<double*>(workspace.regions[checked::kReal].data());
    LAPACK_zporfs(&triangle, &n, &nrhs, values.a, &values.lda, values.af,
                  &values.ldaf, values.b, &values.ldb, values.x, &values.ldx,
                  ferr, berr, work, extra, &info);
    return info;
  }
};

template <typename T>
Status Validate(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, const Values<T>& values) {
  Status status = checked::Triangle(triangle);
  if (!status.ok()) {
    return status;
  }
  for (const auto& matrix :
       {values.a, values.af, values.b,
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
  status = internal_cholesky_expert_counts::System(
      n, values.b.columns(), checked::kIntegerLimit, false);
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
      Native<T>::kName, Native<T>::kKind,
      std::array{
          values.a.rows(), values.a.columns(), checked::Leading(values.a),
          values.af.rows(), values.af.columns(), checked::Leading(values.af),
          values.b.rows(), values.b.columns(), checked::Leading(values.b),
          values.x.rows(), values.x.columns(), checked::Leading(values.x),
          values.ferr.size(), values.berr.size()},
      std::array<std::int64_t, 11>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(values.a.layout()),
          static_cast<std::int64_t>(values.af.layout()),
          static_cast<std::int64_t>(values.b.layout()),
          static_cast<std::int64_t>(values.x.layout()),
          values.a.leading_dimension(), values.af.leading_dimension(),
          values.b.leading_dimension(), values.x.leading_dimension(),
          values.ferr.increment(), values.berr.increment()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const bool active = values.a.rows() != 0 && values.b.columns() != 0;
  status = checked::AddEstimatorWork<T>(active ? values.a.rows() : 0, plan);
  if (!status.ok()) {
    return status;
  }
  if (active) {
    for (const auto& matrix :
         {values.a, values.af, values.b,
          static_cast<DenseBlasMatrixView<const T>>(values.x)}) {
      status = checked::AddPacking(matrix, plan);
      if (!status.ok()) {
        return status;
      }
    }
  }
  return plan;
}

template <typename T>
PackedValues<T> Pack(const Values<T>& values, DenseBlasTriangle triangle,
                     const LapackWorkspace& workspace) {
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  const auto* a = checked::PackTriangle(values.a, triangle, true, cursor);
  const auto* af = checked::PackTriangle(values.af, triangle, false, cursor);
  const auto* b = checked::PackFull(values.b, cursor);
  auto* x = checked::PackFull(values.x, cursor);
  return {a,
          af,
          b,
          x,
          static_cast<lapack_int>(checked::Leading(values.a)),
          static_cast<lapack_int>(checked::Leading(values.af)),
          static_cast<lapack_int>(checked::Leading(values.b)),
          static_cast<lapack_int>(checked::Leading(values.x))};
}

template <typename T>
Status ValidateErrors(const Values<T>& values, LapackReport& report) {
  bool finite = true;
  for (extent_t i = 0; i < values.ferr.size(); ++i) {
    const auto forward = values.ferr.data()[i];
    const auto backward = values.berr.data()[i];
    if (forward < 0 || backward < 0) {
      return checked::ProviderDefect(0, report);
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
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, const Values<T>& values,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, values.Spans());
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kName, report);
  const auto expected = Query(provider, triangle, values);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace,
                                 values.Spans());
  if (!status.ok()) {
    return status;
  }
  if (values.a.rows() == 0 || values.b.columns() == 0) {
    for (extent_t i = 0; i < values.ferr.size(); ++i) {
      values.ferr.data()[i] = 0;
      values.berr.data()[i] = 0;
    }
    return checked::Complete(report);
  }
  status = checked::ZeroFactor(values.af, report);
  if (!status.ok()) {
    return status;
  }
  const auto packed = Pack(values, triangle, workspace);
  report.called_provider = true;
  const auto info =
      Native<T>::Execute(checked::TriangleCharacter(triangle),
                         static_cast<lapack_int>(values.a.rows()),
                         static_cast<lapack_int>(values.b.columns()), packed,
                         values.ferr.data(), values.berr.data(), workspace);
  report.native_info = info;
  if (info != 0) {
    return checked::ProviderDefect(info, report);
  }
  status = ValidateErrors(values, report);
  if (status.code() != ErrorCode::kProvider) {
    checked::PublishFull(packed.x, values.x);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryPorfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<float> values{original, factors,       rhs,
                             solution, forward_error, backward_error};
  return Query(provider, triangle, values);
}

Status Porfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const float> original,
             DenseBlasMatrixView<const float> factors,
             DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<float> values{original, factors,       rhs,
                             solution, forward_error, backward_error};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPorfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<double> values{original, factors,       rhs,
                              solution, forward_error, backward_error};
  return Query(provider, triangle, values);
}

Status Porfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const double> original,
             DenseBlasMatrixView<const double> factors,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<double> values{original, factors,       rhs,
                              solution, forward_error, backward_error};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPorfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<std::complex<float>> values{
      original, factors, rhs, solution, forward_error, backward_error};
  return Query(provider, triangle, values);
}

Status Porfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<float>> original,
             DenseBlasMatrixView<const std::complex<float>> factors,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<float>> values{
      original, factors, rhs, solution, forward_error, backward_error};
  return Execute(provider, triangle, values, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPorfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<std::complex<double>> values{
      original, factors, rhs, solution, forward_error, backward_error};
  return Query(provider, triangle, values);
}

Status Porfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<double>> original,
             DenseBlasMatrixView<const std::complex<double>> factors,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Values<std::complex<double>> values{
      original, factors, rhs, solution, forward_error, backward_error};
  return Execute(provider, triangle, values, plan, workspace, report);
}

}  // namespace asc
