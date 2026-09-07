#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
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
#include "asc/dense/providers/lapack_lu_band.h"
#include "internal_indefinite.h"
#include "internal_lu_band_limits.h"

namespace asc {
namespace {

// Reuse existing neutral metadata/workspace/RHS helpers and audited private
// ABI declarations. No Bunch-Kaufman factor or pivot algorithm is reused.
namespace common = internal_indefinite;
namespace limits = internal_lu_band_limits;

template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::array<std::string_view, 2> kNames{"sgbtrf", "sgbtrs"};
  static constexpr auto kFactor = LAPACK_sgbtrf;
  static constexpr auto kSolve = LAPACK_sgbtrs_base;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::array<std::string_view, 2> kNames{"dgbtrf", "dgbtrs"};
  static constexpr auto kFactor = LAPACK_dgbtrf;
  static constexpr auto kSolve = LAPACK_dgbtrs_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::array<std::string_view, 2> kNames{"cgbtrf", "cgbtrs"};
  static constexpr auto kFactor = LAPACK_cgbtrf;
  static constexpr auto kSolve = LAPACK_cgbtrs_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::array<std::string_view, 2> kNames{"zgbtrf", "zgbtrs"};
  static constexpr auto kFactor = LAPACK_zgbtrf;
  static constexpr auto kSolve = LAPACK_zgbtrs_base;
};

template <typename Integer>
Status PivotMetadata(const ReferenceLapackProvider& provider,
                     DenseBlasVectorView<Integer> pivots, extent_t count) {
  if (pivots.size() != count || pivots.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  return common::Accessible(provider, pivots.reachable_storage());
}

template <typename Integer>
Status PivotValues(const Integer* pivots, extent_t m, extent_t n, extent_t kl) {
  for (extent_t j = 0; j < std::min(m, n); ++j) {
    const index_t p = pivots[j];
    const index_t upper = j + 1 + std::min(kl, m - j - 1);
    if (p < j + 1 || p > upper) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  return Status::Ok();
}

template <typename T>
Status Band(const ReferenceLapackProvider& provider, LapackLuBandView<T> band) {
  auto status =
      common::Accessible(provider, band.storage().reachable_storage());
  if (!status.ok()) {
    return status;
  }
  return limits::Factor(band.rows(), band.columns(), band.lower_bandwidth(),
                        band.upper_bandwidth(),
                        band.storage().leading_dimension(),
                        common::kIntegerLimit);
}

bool Operation(DenseBlasTranspose operation) {
  return operation == DenseBlasTranspose::kNone ||
         operation == DenseBlasTranspose::kTranspose ||
         operation == DenseBlasTranspose::kConjugateTranspose;
}

char Transpose(DenseBlasTranspose operation) {
  if (operation == DenseBlasTranspose::kNone) {
    return 'N';
  }
  return operation == DenseBlasTranspose::kTranspose ? 'T' : 'C';
}

template <typename T>
extent_t RhsLeading(DenseBlasMatrixView<T> rhs) {
  // GBTRS validates LDB>=max(1,N) before its NRHS=0 quick return.
  return rhs.columns() == 0 ? std::max<extent_t>(1, rhs.rows())
                            : common::Leading(rhs);
}

void Integers(extent_t count, LapackWorkspacePlan& plan) {
  plan.regions[common::kPivot] = {count, count, sizeof(lapack_int),
                                  alignof(lapack_int)};
}

template <typename T>
Result<LapackWorkspacePlan> QueryFactor(const ReferenceLapackProvider& provider,
                                        LapackLuBandView<T> band,
                                        DenseBlasVectorView<index_t> pivots) {
  const std::array operands{band.storage().reachable_storage(),
                            pivots.reachable_storage(),
                            common::Object(provider)};
  for (const auto& status :
       {Band(provider, band),
        PivotMetadata(provider, pivots, std::min(band.rows(), band.columns())),
        common::Disjoint(operands)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kNames[0], Native<T>::kScalar,
      std::array{band.rows(), band.columns(), band.lower_bandwidth(),
                 band.upper_bandwidth(), band.storage().leading_dimension()},
      std::array<std::int64_t, 0>{}, provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  Integers(std::min(band.rows(), band.columns()), plan);
  const auto total = common::Total(plan);
  if (!total.ok()) {
    return total;
  }
  return plan;
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(const ReferenceLapackProvider& provider,
                                       DenseBlasTranspose operation,
                                       ReferenceLuBandFactorView<T> factor,
                                       DenseBlasMatrixView<T> rhs) {
  if (!Operation(operation)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  const auto band = factor.factors();
  if (band.rows() != band.columns() || band.rows() != rhs.rows()) {
    return Status(ErrorCode::kShape);
  }
  const std::array operands{band.storage().reachable_storage(),
                            factor.pivots().reachable_storage(),
                            rhs.reachable_storage(), common::Object(provider)};
  for (const auto& status :
       {common::Accessible(provider, band.storage().reachable_storage()),
        PivotMetadata(provider, factor.pivots(), band.rows()),
        common::Matrix(provider, rhs), common::Disjoint(operands),
        limits::Solve(band.rows(), band.lower_bandwidth(),
                      band.upper_bandwidth(),
                      band.storage().leading_dimension(), rhs.columns(),
                      RhsLeading(rhs), common::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kNames[1], Native<T>::kScalar,
      std::array{band.rows(), band.lower_bandwidth(), band.upper_bandwidth(),
                 band.storage().leading_dimension(), rhs.columns(),
                 RhsLeading(rhs)},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(operation),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (band.rows() != 0 && rhs.columns() != 0) {
    Integers(band.rows(), plan);
    const auto packing = common::Packing(rhs, plan);
    if (!packing.ok()) {
      return packing;
    }
  }
  return plan;
}

template <typename T>
Status Factor(const ReferenceLapackProvider& provider, LapackLuBandView<T> band,
              DenseBlasVectorView<index_t> pivots,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  const std::array operands{band.storage().reachable_storage(),
                            pivots.reachable_storage()};
  auto status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kNames[0], report);
  const auto expected = QueryFactor(provider, band, pivots);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const auto m = static_cast<lapack_int>(band.rows());
  const auto n = static_cast<lapack_int>(band.columns());
  const auto kl = static_cast<lapack_int>(band.lower_bandwidth());
  const auto ku = static_cast<lapack_int>(band.upper_bandwidth());
  const auto ld = static_cast<lapack_int>(band.storage().leading_dimension());
  const auto count = std::min(m, n);
  T dummy{};
  lapack_int dummy_pivot = 0;
  auto* native = count == 0 ? &dummy_pivot
                            : ::new (workspace.regions[common::kPivot].data())
                                  lapack_int[static_cast<std::size_t>(count)];
  std::fill_n(native, count, std::numeric_limits<lapack_int>::min());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native<T>::kFactor(&m, &n, &kl, &ku,
                     count == 0 ? &dummy : band.storage().data(), &ld, native,
                     &info);
  report.native_info = info;
  if (info < 0 || info > count ||
      !PivotValues(native, band.rows(), band.columns(), band.lower_bandwidth())
           .ok()) {
    return common::Defect(info, report);
  }
  if (count != 0) {
    std::copy_n(native, count, pivots.data());
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation, ReferenceLuBandFactorView<T> factor,
             DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  const auto band = factor.factors();
  const auto pivots = factor.pivots();
  const std::array operands{band.storage().reachable_storage(),
                            pivots.reachable_storage(),
                            rhs.reachable_storage()};
  auto status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kNames[1], report);
  const auto expected = QuerySolve(provider, operation, factor, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  status = PivotValues(pivots.data(), band.rows(), band.columns(),
                       band.lower_bandwidth());
  if (!status.ok()) {
    return status;
  }
  const bool active = band.rows() != 0 && rhs.columns() != 0;
  if (active) {
    for (extent_t j = 0; j < band.rows(); ++j) {
      if (band.storage().data()[j * band.storage().leading_dimension() +
                                band.diagonal_row()] == T{}) {
        report.outcome = LapackOutcome::kSingular;
        report.diagnostic_index = j;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  const auto n = static_cast<lapack_int>(band.rows());
  const auto kl = static_cast<lapack_int>(band.lower_bandwidth());
  const auto ku = static_cast<lapack_int>(band.upper_bandwidth());
  const auto nrhs = static_cast<lapack_int>(rhs.columns());
  const auto ld = static_cast<lapack_int>(band.storage().leading_dimension());
  const auto ldb = static_cast<lapack_int>(RhsLeading(rhs));
  lapack_int dummy_pivot = 0;
  auto* native = !active ? &dummy_pivot
                         : ::new (workspace.regions[common::kPivot].data())
                               lapack_int[static_cast<std::size_t>(n)];
  if (active) {
    for (extent_t i = 0; i < band.rows(); ++i) {
      native[i] = static_cast<lapack_int>(pivots.data()[i]);
    }
  }
  T dummy{};
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  auto* packed = active ? common::PackRhs(rhs, cursor) : &dummy;
  const char trans = Transpose(operation);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native<T>::kSolve(&trans, &n, &kl, &ku, &nrhs,
                    active ? band.storage().data() : &dummy, &ld, native,
                    packed, &ldb, &info
#ifdef LAPACK_FORTRAN_STRLEN_END
                    ,
                    FORTRAN_STRLEN{1}
#endif
  );
  report.native_info = info;
  if (info != 0) {
    return common::Defect(info, report);
  }
  if (active) {
    common::PublishRhs(packed, rhs);
  }
  return common::Complete(report);
}

}  // namespace

template <DenseBlasScalar Element>
Result<ReferenceLuBandFactorView<Element>>
ReferenceLuBandFactorView<Element>::Create(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<const Element> factors,
    DenseBlasVectorView<const index_t> pivots, const LapackReport& report) {
  std::array<char, 32> origin{};
  std::copy(Native<Element>::kNames[0].begin(),
            Native<Element>::kNames[0].end(), origin.begin());
  if (report.provider != provider.identity() || report.routine != origin ||
      !report.called_provider || report.native_info != 0 ||
      report.outcome != LapackOutcome::kSuccess ||
      report.output_validity != LapackOutputValidity::kComplete ||
      report.factor_family.has_value() || report.native_argument.has_value() ||
      report.diagnostic_index.has_value()) {
    return Status(ErrorCode::kInvalidState);
  }
  const std::array operands{factors.storage().reachable_storage(),
                            pivots.reachable_storage(),
                            common::Object(provider), common::Object(report)};
  for (const auto& status :
       {Band(provider, factors),
        PivotMetadata(provider, pivots,
                      std::min(factors.rows(), factors.columns())),
        common::Disjoint(operands)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto status = PivotValues(pivots.data(), factors.rows(),
                                  factors.columns(), factors.lower_bandwidth());
  if (!status.ok()) {
    return status;
  }
  return ReferenceLuBandFactorView(factors, pivots, report);
}

template class ReferenceLuBandFactorView<float>;
template class ReferenceLuBandFactorView<double>;
template class ReferenceLuBandFactorView<std::complex<float>>;
template class ReferenceLuBandFactorView<std::complex<double>>;

Result<LapackWorkspacePlan> QueryGbtrfWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<float> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots);
}

Status Gbtrf(const ReferenceLapackProvider& provider,
             LapackLuBandView<float> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
    ReferenceLuBandFactorView<float> factor, DenseBlasMatrixView<float> rhs) {
  return QuerySolve(provider, operation, factor, rhs);
}

Status Gbtrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation,
             ReferenceLuBandFactorView<float> factor,
             DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, operation, factor, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGbtrfWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<double> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots);
}

Status Gbtrf(const ReferenceLapackProvider& provider,
             LapackLuBandView<double> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
    ReferenceLuBandFactorView<double> factor, DenseBlasMatrixView<double> rhs) {
  return QuerySolve(provider, operation, factor, rhs);
}

Status Gbtrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation,
             ReferenceLuBandFactorView<double> factor,
             DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, operation, factor, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots);
}

Status Gbtrf(const ReferenceLapackProvider& provider,
             LapackLuBandView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
    ReferenceLuBandFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QuerySolve(provider, operation, factor, rhs);
}

Status Gbtrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation,
             ReferenceLuBandFactorView<std::complex<float>> factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, operation, factor, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots);
}

Status Gbtrf(const ReferenceLapackProvider& provider,
             LapackLuBandView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
    ReferenceLuBandFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QuerySolve(provider, operation, factor, rhs);
}

Status Gbtrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation,
             ReferenceLuBandFactorView<std::complex<double>> factor,
             DenseBlasMatrixView<std::complex<double>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, operation, factor, rhs, plan, workspace, report);
}

}  // namespace asc
