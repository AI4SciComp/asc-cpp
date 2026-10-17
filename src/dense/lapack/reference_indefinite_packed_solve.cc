#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating caller INTEGER lifetime.
#include <string_view>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed_solve.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_packed_solve_counts.h"
#include "internal_packed_triangular.h"

namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace packed = internal_packed_triangular;

template <typename T>
std::string_view Name(bool hermitian) {
  if constexpr (std::is_same_v<T, float>) {
    return "SSPTRS";
  } else if constexpr (std::is_same_v<T, double>) {
    return "DSPTRS";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "CHPTRS" : "CSPTRS";
  } else {
    return hermitian ? "ZHPTRS" : "ZSPTRS";
  }
}

template <typename T>
Status Metadata(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle,
                DenseBlasPackedMatrixView<const T> factors,
                RawLapackPivotView pivots, DenseBlasMatrixView<T> rhs) {
  if (!bk::Triangle(triangle) ||
      pivots.family() != LapackFactorFamily::kBunchKaufman) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.order() != rhs.rows() ||
      pivots.values().size() != static_cast<std::size_t>(factors.order())) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{factors.reachable_storage(),
                         pivots.reachable_storage(), rhs.reachable_storage(),
                         bk::Object(provider)};
  for (const Status& status :
       {bk::Accessible(provider, factors.reachable_storage()),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Matrix(provider, rhs), bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return internal_indefinite_packed_solve_counts::Solve(
      factors.order(), rhs.columns(), bk::Leading(rhs), bk::kIntegerLimit);
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasPackedMatrixView<const T> factors,
                                  RawLapackPivotView pivots,
                                  DenseBlasMatrixView<T> rhs, bool hermitian) {
  Status metadata = Metadata(provider, triangle, factors, pivots, rhs);
  if (!metadata.ok()) {
    return metadata;
  }
  const std::array dimensions{factors.order(), rhs.columns(), bk::Leading(rhs)};
  const std::array<std::int64_t, 6> options{
      static_cast<std::int64_t>(triangle),
      static_cast<std::int64_t>(hermitian),
      static_cast<std::int64_t>(factors.layout()),
      static_cast<std::int64_t>(pivots.values().size()),
      static_cast<std::int64_t>(rhs.layout()),
      rhs.leading_dimension()};
  const auto identity =
      LapackPlanIdentity::Create(Name<T>(hermitian), bk::ScalarKind<T>(),
                                 dimensions, options, provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (factors.order() == 0 || rhs.columns() == 0) {
    return plan;
  }
  plan.regions[bk::kPivot] = {factors.order(), factors.order(),
                              sizeof(lapack_int), alignof(lapack_int)};
  for (const Status& status :
       {packed::Packing(factors, plan), bk::Packing(rhs, plan)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

template <typename T>
T Entry(DenseBlasPackedMatrixView<const T> factors, DenseBlasTriangle triangle,
        extent_t i, extent_t j) {
  return factors.data()[packed::Offset(factors.order(), triangle,
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
      zero = hermitian ? bk::Real(diagonal) == 0 : diagonal == T{};
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
void Native(bool hermitian, char triangle, lapack_int n, lapack_int nrhs,
            const T* a, const lapack_int* pivots, T* b, lapack_int ldb,
            lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssptrs(&triangle, &n, &nrhs, a, pivots, b, &ldb, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsptrs(&triangle, &n, &nrhs, a, pivots, b, &ldb, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chptrs(&triangle, &n, &nrhs, a, pivots, b, &ldb, &info);
    } else {
      LAPACK_csptrs(&triangle, &n, &nrhs, a, pivots, b, &ldb, &info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhptrs(&triangle, &n, &nrhs, a, pivots, b, &ldb, &info);
    } else {
      LAPACK_zsptrs(&triangle, &n, &nrhs, a, pivots, b, &ldb, &info);
    }
  }
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasPackedMatrixView<const T> factors,
               RawLapackPivotView pivots, DenseBlasMatrixView<T> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool hermitian) {
  const std::array operands{factors.reachable_storage(),
                            pivots.reachable_storage(),
                            rhs.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(hermitian), report);
  const auto expected =
      Query(provider, triangle, factors, pivots, rhs, hermitian);
  if (!expected.ok()) {
    return expected.status();
  }
  status = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (factors.order() == 0 || rhs.columns() == 0) {
    return bk::Complete(report);
  }
  status = bk::Paired(pivots.values(), factors.order(), triangle);
  if (!status.ok()) {
    return status;
  }
  status = BlockDivisors(factors, triangle, pivots, hermitian, report);
  if (!status.ok()) {
    return status;
  }
  auto* native_pivots = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(factors.order())];
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    native_pivots[i] = static_cast<lapack_int>(pivots.values()[i]);
  }
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const T* native_factors =
      packed::Pack(factors, triangle, DenseBlasDiagonal::kNonUnit, cursor);
  T* native_rhs = bk::PackRhs(rhs, cursor);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native(hermitian, bk::Uplo(triangle),
         static_cast<lapack_int>(factors.order()),
         static_cast<lapack_int>(rhs.columns()), native_factors, native_pivots,
         native_rhs, static_cast<lapack_int>(bk::Leading(rhs)), info);
  report.native_info = info;
  if (info != 0) {
    return bk::Defect(info, report);
  }
  bk::PublishRhs(native_rhs, rhs);
  return bk::Complete(report);
}
}  // namespace

Result<LapackWorkspacePlan> QuerySptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status Sptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const float> factors,
             RawLapackPivotView pivots, DenseBlasMatrixView<float> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}

Result<LapackWorkspacePlan> QuerySptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status Sptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const double> factors,
             RawLapackPivotView pivots, DenseBlasMatrixView<double> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}

Result<LapackWorkspacePlan> QuerySptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status Sptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}

Result<LapackWorkspacePlan> QuerySptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status Sptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<std::complex<double>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}

Result<LapackWorkspacePlan> QueryHptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, true);
}
Status Hptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, true);
}

Result<LapackWorkspacePlan> QueryHptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, true);
}
Status Hptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<std::complex<double>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, true);
}

}  // namespace asc
