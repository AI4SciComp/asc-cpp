#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band_driver.h"
#include "internal_indefinite.h"
#include "internal_layout.h"
#include "internal_lu_band_expert.h"
#include "internal_lu_band_limits.h"
namespace asc {
namespace {
namespace checked = internal_lu_band_expert;
namespace common = internal_indefinite;
namespace limits = internal_lu_band_limits;
namespace layout = internal_lapack_layout;
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sgbsv";
  static constexpr auto kExecute = LAPACK_sgbsv;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dgbsv";
  static constexpr auto kExecute = LAPACK_dgbsv;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cgbsv";
  static constexpr auto kExecute = LAPACK_cgbsv;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zgbsv";
  static constexpr auto kExecute = LAPACK_zgbsv;
};
template <typename T>
auto Operands(LapackLuBandView<T> matrix, DenseBlasVectorView<index_t> pivots,
              DenseBlasMatrixView<T> rhs) {
  return std::array{matrix.storage().reachable_storage(),
                    pivots.reachable_storage(), rhs.reachable_storage()};
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  LapackLuBandView<T> matrix,
                                  DenseBlasVectorView<index_t> pivots,
                                  DenseBlasMatrixView<T> rhs) {
  const auto n = matrix.rows();
  const auto kl = matrix.lower_bandwidth();
  const auto ku = matrix.upper_bandwidth();
  const auto ld = matrix.storage().leading_dimension();
  if (matrix.columns() != n || rhs.rows() != n) {
    return Status(ErrorCode::kShape);
  }
  const auto operands = Operands(matrix, pivots, rhs);
  for (const auto& status :
       {common::Accessible(provider, operands[0]),
        checked::Vector(provider, pivots, n),
        common::Accessible(provider, operands[2]),
        common::Disjoint(std::array{operands[0], operands[1], operands[2],
                                    common::Object(provider)}),
        limits::Factor(n, n, kl, ku, ld, common::kIntegerLimit),
        limits::Solve(n, kl, ku, ld, rhs.columns(), checked::RhsLeading(rhs),
                      common::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{n, kl, ku, ld, rhs.columns(), checked::RhsLeading(rhs)},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  plan.regions[common::kPivot] = {n, n, sizeof(lapack_int),
                                  alignof(lapack_int)};
  const auto status = layout::AddPacking(rhs, plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}
template <typename T>
lapack_int Call(LapackLuBandView<T> matrix, DenseBlasMatrixView<T> rhs, T* b,
                lapack_int* pivots) {
  const auto n = static_cast<lapack_int>(matrix.rows());
  const auto kl = static_cast<lapack_int>(matrix.lower_bandwidth());
  const auto ku = static_cast<lapack_int>(matrix.upper_bandwidth());
  const auto ld = static_cast<lapack_int>(matrix.storage().leading_dimension());
  const auto nrhs = static_cast<lapack_int>(rhs.columns());
  const auto ldb = static_cast<lapack_int>(checked::RhsLeading(rhs));
  T dummy{};
  lapack_int info = std::numeric_limits<lapack_int>::min();
  Native<T>::kExecute(&n, &kl, &ku, &nrhs,
                      checked::Nonnull(matrix.storage().data(), dummy), &ld,
                      pivots, checked::Nonnull(b, dummy), &ldb, &info);
  return info;
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackLuBandView<T> matrix, DenseBlasVectorView<index_t> pivots,
               DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = Operands(matrix, pivots, rhs);
  auto status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kName, report);
  const auto expected = Query(provider, matrix, pivots, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  lapack_int dummy = std::numeric_limits<lapack_int>::min();
  auto* integers = checked::Integers(workspace, matrix.rows(), dummy);
  std::fill_n(integers, matrix.rows(), std::numeric_limits<lapack_int>::min());
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  auto* b = layout::Pack(rhs, cursor);
  report.called_provider = true;
  const auto info = Call(matrix, rhs, b, integers);
  report.native_info = info;
  if (info < 0 || info > matrix.rows() ||
      !checked::Pivots(std::span<const lapack_int>(
                           integers, static_cast<std::size_t>(matrix.rows())),
                       matrix.rows(), matrix.lower_bandwidth())
           .ok()) {
    return common::Defect(info, report);
  }
  std::copy_n(integers, matrix.rows(), pivots.data());
  if (info > 0) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  layout::Unpack(b, rhs);
  return common::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryGbsvWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<float> matrix,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<float> rhs) {
  return Query(provider, matrix, pivots, rhs);
}
Status Gbsv(const ReferenceLapackProvider& provider,
            LapackLuBandView<float> matrix, DenseBlasVectorView<index_t> pivots,
            DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, matrix, pivots, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<double> matrix,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<double> rhs) {
  return Query(provider, matrix, pivots, rhs);
}
Status Gbsv(const ReferenceLapackProvider& provider,
            LapackLuBandView<double> matrix,
            DenseBlasVectorView<index_t> pivots,
            DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, matrix, pivots, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, matrix, pivots, rhs);
}
Status Gbsv(const ReferenceLapackProvider& provider,
            LapackLuBandView<std::complex<float>> matrix,
            DenseBlasVectorView<index_t> pivots,
            DenseBlasMatrixView<std::complex<float>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, matrix, pivots, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, matrix, pivots, rhs);
}
Status Gbsv(const ReferenceLapackProvider& provider,
            LapackLuBandView<std::complex<double>> matrix,
            DenseBlasVectorView<index_t> pivots,
            DenseBlasMatrixView<std::complex<double>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, matrix, pivots, rhs, plan, workspace, report);
}
}  // namespace asc
