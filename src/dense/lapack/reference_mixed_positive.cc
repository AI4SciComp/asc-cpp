#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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
#include "asc/dense/providers/lapack_mixed_general.h"
#include "asc/dense/providers/lapack_mixed_positive.h"
#include "internal_cholesky_expert.h"
#include "internal_layout.h"
#include "internal_mixed_positive_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace layout = internal_lapack_layout;
namespace counts = internal_mixed_positive_counts;
namespace triangle = internal_cholesky_expert;

template <typename T>
using Low = std::conditional_t<DenseBlasComplex<T>, std::complex<float>, float>;
template <typename T>
constexpr std::string_view Routine() {
  return DenseBlasComplex<T> ? "zcposv" : "dsposv";
}
template <typename T>
struct Operands {
  DenseBlasMatrixView<T> a;
  DenseBlasTriangle uplo;
  DenseBlasMatrixView<const T> b;
  DenseBlasMatrixView<T> x;
  [[nodiscard]] auto Spans() const {
    return std::array<ConstMemoryView, 3>{
        a.reachable_storage(), b.reachable_storage(), x.reachable_storage()};
  }
};
template <typename T>
Result<counts::Counts> Metadata(const ReferenceLapackProvider& provider,
                                const Operands<T>& operands) {
  const auto n = operands.a.rows();
  if (operands.a.columns() != n || operands.b.rows() != n ||
      operands.x.rows() != n || operands.x.columns() != operands.b.columns()) {
    return Status(ErrorCode::kShape);
  }
  Status status = triangle::Triangle(operands.uplo);
  if (status.ok()) {
    status = triangle::Matrix(provider, operands.a, DenseBlasComplex<T>);
  }
  if (status.ok()) {
    status = checked::Matrix(provider, operands.b, n);
  }
  if (status.ok()) {
    status = checked::Matrix(provider, operands.x, n);
  }
  if (status.ok()) {
    status = checked::Disjoint(operands.Spans());
  }
  if (!status.ok()) {
    return status;
  }
  return counts::Query(n, operands.b.columns(),
                       triangle::Leading(operands.a, DenseBlasComplex<T>),
                       checked::kLimit, DenseBlasComplex<T>);
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Operands<T>& operands) {
  const auto sizes = Metadata(provider, operands);
  if (!sizes.ok()) {
    return sizes.status();
  }
  const auto kind = DenseBlasComplex<T> ? LapackScalarKind::kMixedC128C64
                                        : LapackScalarKind::kMixedF64F32;
  const auto key = LapackPlanIdentity::Create(
      Routine<T>(), kind,
      std::array{operands.a.rows(), operands.b.columns(),
                 triangle::Leading(operands.a, DenseBlasComplex<T>),
                 checked::Leading(operands.b)},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(operands.uplo),
          static_cast<std::int64_t>(operands.a.layout()),
          operands.a.leading_dimension(),
          static_cast<std::int64_t>(operands.b.layout()),
          operands.b.leading_dimension(),
          static_cast<std::int64_t>(operands.x.layout()),
          operands.x.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (operands.a.rows() == 0) {
    return plan;
  }
  plan.regions[checked::kScalar] = {sizes->residual, sizes->residual, sizeof(T),
                                    alignof(T)};
  plan.regions[checked::kScratch] = {sizes->lower, sizes->lower, sizeof(Low<T>),
                                     alignof(Low<T>)};
  plan.regions[checked::kLayout] = {sizes->solution, sizes->solution, sizeof(T),
                                    alignof(T)};
  if constexpr (DenseBlasComplex<T>) {
    const auto n = operands.a.rows();
    plan.regions[checked::kReal] = {n, n, sizeof(double), alignof(double)};
  }
  Status status = triangle::AddPacking(operands.a, plan, DenseBlasComplex<T>);
  if (status.ok()) {
    status = layout::AddPacking(operands.b, plan);
  }
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}
std::optional<LapackMixedFallback> Fallback(lapack_int iter) {
  if (iter >= 0 && iter <= 30) {
    return LapackMixedFallback::kNone;
  }
  switch (iter) {
    case -1:
      return LapackMixedFallback::kImplementation;
    case -2:
      return LapackMixedFallback::kConversionRange;
    case -3:
      return LapackMixedFallback::kLowFactorization;
    case -31:
      return LapackMixedFallback::kIterationLimit;
    default:
      return std::nullopt;
  }
}
template <typename T>
void Native(char uplo, lapack_int n, lapack_int nrhs, T* a, lapack_int lda,
            const T* b, lapack_int ldb, T* x, T* work, Low<T>* lower,
            double* real, lapack_int& iter, lapack_int& info) {
  if constexpr (DenseBlasComplex<T>) {
    LAPACK_zcposv(&uplo, &n, &nrhs, a, &lda, b, &ldb, x, &n, work, lower, real,
                  &iter, &info);
  } else {
    static_cast<void>(real);
    LAPACK_dsposv(&uplo, &n, &nrhs, a, &lda, b, &ldb, x, &n, work, lower, &iter,
                  &info);
  }
}
// X is output-only and already has live caller-supplied staging objects.
// Publish handles either caller layout; the staged stride is exactly N.
template <typename T>
bool Publish(const T* staged, DenseBlasMatrixView<T> x) {
  bool finite = true;
  for (extent_t j = 0; j < x.columns(); ++j) {
    for (extent_t i = 0; i < x.rows(); ++i) {
      const T value = staged[j * x.rows() + i];
      const auto at = x.layout() == DenseBlasLayout::kRowMajor
                          ? i * x.leading_dimension() + j
                          : j * x.leading_dimension() + i;
      x.data()[at] = value;
      if constexpr (DenseBlasComplex<T>) {
        finite = finite && std::isfinite(value.real()) &&
                 std::isfinite(value.imag());
      } else {
        finite = finite && std::isfinite(value);
      }
    }
  }
  return finite;
}
template <typename T>
Status PublishResults(const Operands<T>& operands, const T* a, const T* x,
                      lapack_int info, lapack_int iter,
                      LapackMixedSolveStatistics& statistics,
                      LapackReport& report) {
  statistics = {iter, Fallback(iter), std::nullopt};
  if (info < 0 || info > operands.a.rows() || !statistics.fallback ||
      (info > 0 && iter >= 0)) {
    return checked::ProviderDefect(info, report);
  }
  if (iter < 0) {
    triangle::PublishTriangle(a, operands.a, operands.uplo, info > 0,
                              DenseBlasComplex<T>);
  }
  statistics.factor_scalar =
      iter < 0 ? checked::Kind<T>() : checked::Kind<Low<T>>();
  if (info > 0) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  if (!Publish(x, operands.x)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  if (iter < 0) {
    report.factor_family = LapackFactorFamily::kCholesky;
  }
  return checked::Complete(report);
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               const Operands<T>& operands, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace,
               LapackMixedSolveStatistics& statistics, LapackReport& report) {
  const auto values = operands.Spans();
  const std::array<ConstMemoryView, 4> spans{
      values[0], values[1], values[2], checked::ObjectStorage(statistics)};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<T>(), report);
  const auto expected = Query(provider, operands);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::Disjoint(spans);
  if (status.ok()) {
    status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  }
  if (!status.ok()) {
    return status;
  }
  const auto n = operands.a.rows();
  if (n == 0) {
    statistics = {};
    return checked::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  T* x = cursor;
  const auto solution_count = n * operands.b.columns();
  if (solution_count > 0) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const T sentinel = [&] {
      if constexpr (DenseBlasComplex<T>) {
        return T{nan, nan};
      } else {
        return nan;
      }
    }();
    std::fill_n(x, static_cast<std::size_t>(solution_count), sentinel);
    cursor += solution_count;
  }
  T* a = triangle::PackTriangle(operands.a, operands.uplo, true, cursor,
                                DenseBlasComplex<T>);
  const T* b = layout::Pack(operands.b, cursor);
  auto* work = static_cast<T*>(workspace.regions[checked::kScalar].data());
  auto* lower =
      static_cast<Low<T>*>(workspace.regions[checked::kScratch].data());
  auto* real = static_cast<double*>(workspace.regions[checked::kReal].data());
  T unused{};
  lapack_int iter = std::numeric_limits<lapack_int>::min();
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native(triangle::TriangleCharacter(operands.uplo), static_cast<lapack_int>(n),
         static_cast<lapack_int>(operands.b.columns()), a,
         static_cast<lapack_int>(
             triangle::Leading(operands.a, DenseBlasComplex<T>)),
         checked::Nonnull<const T>(b, unused),
         static_cast<lapack_int>(checked::Leading(operands.b)),
         checked::Nonnull(x, unused), checked::Nonnull(work, unused), lower,
         real, iter, info);
  report.native_info = info;
  return PublishResults(operands, a, x, info, iter, statistics, report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryDsposvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> a,
    DenseBlasTriangle uplo, DenseBlasMatrixView<const double> b,
    DenseBlasMatrixView<double> x) {
  return Query(provider, Operands<double>{a, uplo, b, x});
}
Status Dsposv(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<double> a, DenseBlasTriangle uplo,
              DenseBlasMatrixView<const double> b,
              DenseBlasMatrixView<double> x, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace,
              LapackMixedSolveStatistics& statistics, LapackReport& report) {
  return Execute(provider, Operands<double>{a, uplo, b, x}, plan, workspace,
                 statistics, report);
}
Result<LapackWorkspacePlan> QueryZcposvWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> a, DenseBlasTriangle uplo,
    DenseBlasMatrixView<const std::complex<double>> b,
    DenseBlasMatrixView<std::complex<double>> x) {
  return Query(provider, Operands<std::complex<double>>{a, uplo, b, x});
}
Status Zcposv(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<std::complex<double>> a,
              DenseBlasTriangle uplo,
              DenseBlasMatrixView<const std::complex<double>> b,
              DenseBlasMatrixView<std::complex<double>> x,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackMixedSolveStatistics& statistics, LapackReport& report) {
  return Execute(provider, Operands<std::complex<double>>{a, uplo, b, x}, plan,
                 workspace, statistics, report);
}
}  // namespace asc
