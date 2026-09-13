#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_refinement.h"
#include "internal_cholesky_expert.h"
#include "internal_indefinite.h"
#include "internal_packed_cholesky_refinement_counts.h"
#include "internal_packed_triangular.h"

namespace asc {
namespace {
namespace checked = internal_cholesky_expert;
namespace common = internal_indefinite;
namespace storage = internal_packed_triangular;

template <typename T>
struct Values {
  DenseBlasPackedMatrixView<const T> a;
  DenseBlasPackedMatrixView<const T> af;
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
  lapack_int ldb;
  lapack_int ldx;
};

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "spprfs";
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<float>& values, float* ferr,
                            float* berr, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work =
        static_cast<float*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<float>(n, workspace);
    LAPACK_spprfs(&triangle, &n, &nrhs, values.a, values.af, values.b,
                  &values.ldb, values.x, &values.ldx, ferr, berr, work, extra,
                  &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dpprfs";
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<double>& values, double* ferr,
                            double* berr, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work =
        static_cast<double*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<double>(n, workspace);
    LAPACK_dpprfs(&triangle, &n, &nrhs, values.a, values.af, values.b,
                  &values.ldb, values.x, &values.ldx, ferr, berr, work, extra,
                  &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cpprfs";
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<std::complex<float>>& values,
                            float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work = static_cast<std::complex<float>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra = static_cast<float*>(workspace.regions[checked::kReal].data());
    LAPACK_cpprfs(&triangle, &n, &nrhs, values.a, values.af, values.b,
                  &values.ldb, values.x, &values.ldx, ferr, berr, work, extra,
                  &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zpprfs";
  static lapack_int Execute(char triangle, lapack_int n, lapack_int nrhs,
                            const PackedValues<std::complex<double>>& values,
                            double* ferr, double* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work = static_cast<std::complex<double>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra =
        static_cast<double*>(workspace.regions[checked::kReal].data());
    LAPACK_zpprfs(&triangle, &n, &nrhs, values.a, values.af, values.b,
                  &values.ldb, values.x, &values.ldx, ferr, berr, work, extra,
                  &info);
    return info;
  }
};

template <typename T>
Status Validate(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, const Values<T>& values) {
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
    if (vector.size() != values.b.columns()) {
      return Status(ErrorCode::kShape);
    }
    if (vector.increment() != 1) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  Status status = common::Disjoint(values.Spans());
  if (!status.ok()) {
    return status;
  }
  const bool active = n != 0 && values.b.columns() != 0;
  return internal_packed_cholesky_refinement_counts::Refine(
      n, values.b.columns(), active ? common::Leading(values.b) : 1,
      active ? common::Leading(values.x) : 1, common::kIntegerLimit);
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  const Values<T>& values) {
  Status status = Validate(provider, triangle, values);
  if (!status.ok()) {
    return status;
  }
  const bool active = values.a.order() != 0 && values.b.columns() != 0;
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kKind,
      std::array{values.a.order(), values.b.columns(),
                 active ? common::Leading(values.b) : 1,
                 active ? common::Leading(values.x) : 1, values.ferr.size(),
                 values.berr.size()},
      std::array<std::int64_t, 9>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(values.a.layout()),
          static_cast<std::int64_t>(values.af.layout()),
          static_cast<std::int64_t>(values.b.layout()),
          static_cast<std::int64_t>(values.x.layout()),
          values.b.leading_dimension(), values.x.leading_dimension(),
          values.ferr.increment(), values.berr.increment()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (!active) {
    return plan;
  }
  status = checked::AddEstimatorWork<T>(values.a.order(), plan);
  if (!status.ok()) {
    return status;
  }
  for (const auto& matrix : {values.a, values.af}) {
    status = storage::Packing(matrix, plan);
    if (!status.ok()) {
      return status;
    }
  }
  for (const auto& matrix :
       {values.b, static_cast<DenseBlasMatrixView<const T>>(values.x)}) {
    status = common::Packing(matrix, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

template <typename T>
const T* PackOriginal(DenseBlasPackedMatrixView<const T> a,
                      DenseBlasTriangle triangle, T*& cursor) {
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
        if (i == j) {
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
                     const LapackWorkspace& workspace) {
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  const auto* a = PackOriginal(values.a, triangle, cursor);
  const auto* af =
      storage::Pack(values.af, triangle, DenseBlasDiagonal::kNonUnit, cursor);
  const auto* b = checked::PackFull(values.b, cursor);
  auto* x = checked::PackFull(values.x, cursor);
  return {a,
          af,
          b,
          x,
          static_cast<lapack_int>(common::Leading(values.b)),
          static_cast<lapack_int>(common::Leading(values.x))};
}

template <typename T>
Status ValidateErrors(const Values<T>& values, LapackReport& report) {
  bool finite = true;
  for (extent_t i = 0; i < values.ferr.size(); ++i) {
    const auto forward = values.ferr.data()[i];
    const auto backward = values.berr.data()[i];
    if (forward < 0 || backward < 0) {
      return common::Defect(0, report);
    }
    finite = finite && std::isfinite(forward) && std::isfinite(backward);
  }
  if (!finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, const Values<T>& values,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  Status status =
      common::Metadata(provider, plan, workspace, report, values.Spans());
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kName, report);
  const auto expected = Query(provider, triangle, values);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, values.Spans());
  if (!status.ok()) {
    return status;
  }
  if (values.a.order() == 0 || values.b.columns() == 0) {
    for (extent_t i = 0; i < values.ferr.size(); ++i) {
      values.ferr.data()[i] = 0;
      values.berr.data()[i] = 0;
    }
    return common::Complete(report);
  }
  const auto packed = Pack(values, triangle, workspace);
  for (extent_t j = 0; j < values.ferr.size(); ++j) {
    using Real = DenseBlasRealType<T>;
    values.ferr.data()[j] = std::numeric_limits<Real>::quiet_NaN();
    values.berr.data()[j] = std::numeric_limits<Real>::quiet_NaN();
  }
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      common::Uplo(triangle), static_cast<lapack_int>(values.a.order()),
      static_cast<lapack_int>(values.b.columns()), packed, values.ferr.data(),
      values.berr.data(), workspace);
  report.native_info = info;
  if (info != 0) {
    return common::Defect(info, report);
  }
  status = ValidateErrors(values, report);
  if (status.code() != ErrorCode::kProvider) {
    checked::PublishFull(packed.x, values.x);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryPprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<float> values{original, factors,       rhs,
                             solution, forward_error, backward_error};
  return Query(provider, triangle, values);
}

Status Pprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const float> original,
             DenseBlasPackedMatrixView<const float> factors,
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

Result<LapackWorkspacePlan> QueryPprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<double> values{original, factors,       rhs,
                              solution, forward_error, backward_error};
  return Query(provider, triangle, values);
}

Status Pprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const double> original,
             DenseBlasPackedMatrixView<const double> factors,
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

Result<LapackWorkspacePlan> QueryPprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  const Values<std::complex<float>> values{
      original, factors, rhs, solution, forward_error, backward_error};
  return Query(provider, triangle, values);
}

Status Pprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> original,
             DenseBlasPackedMatrixView<const std::complex<float>> factors,
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

Result<LapackWorkspacePlan> QueryPprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  const Values<std::complex<double>> values{
      original, factors, rhs, solution, forward_error, backward_error};
  return Query(provider, triangle, values);
}

Status Pprfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> original,
             DenseBlasPackedMatrixView<const std::complex<double>> factors,
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
