#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
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
#include "asc/dense/providers/lapack_indefinite_rk_inverse.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_expert.h"
#include "internal_indefinite_rk_inverse_counts.h"
#include "internal_indefinite_rk_inverse_prototypes.h"

namespace asc {
namespace {
namespace bk = internal_indefinite;
using internal_indefinite_rk_inverse_counts::Options;

template <typename T>
std::string_view Name(Options options) {
  if constexpr (std::is_same_v<T, float>) {
    return options.explicit_block ? "ssytri_3x" : "ssytri_3";
  } else if constexpr (std::is_same_v<T, double>) {
    return options.explicit_block ? "dsytri_3x" : "dsytri_3";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (options.hermitian) {
      return options.explicit_block ? "chetri_3x" : "chetri_3";
    }
    return options.explicit_block ? "csytri_3x" : "csytri_3";
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (options.hermitian) {
      return options.explicit_block ? "zhetri_3x" : "zhetri_3";
    }
    return options.explicit_block ? "zsytri_3x" : "zsytri_3";
  }
}
template <typename T>
void Call(Options options, char triangle, lapack_int n, T* a, lapack_int lda,
          const T* off_diagonal, lapack_int* pivots, T* work, lapack_int size,
          lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    if (options.explicit_block) {
      ssytri_3x_(&triangle, &n, a, &lda, const_cast<T*>(off_diagonal), pivots,
                 work, &size, &info, 1);
    } else {
      LAPACK_ssytri_3(&triangle, &n, a, &lda, off_diagonal, pivots, work, &size,
                      &info);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (options.explicit_block) {
      dsytri_3x_(&triangle, &n, a, &lda, const_cast<T*>(off_diagonal), pivots,
                 work, &size, &info, 1);
    } else {
      LAPACK_dsytri_3(&triangle, &n, a, &lda, off_diagonal, pivots, work, &size,
                      &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (options.hermitian) {
      if (options.explicit_block) {
        chetri_3x_(&triangle, &n, a, &lda, const_cast<T*>(off_diagonal), pivots,
                   work, &size, &info, 1);
      } else {
        LAPACK_chetri_3(&triangle, &n, a, &lda, off_diagonal, pivots, work,
                        &size, &info);
      }
    } else {
      if (options.explicit_block) {
        csytri_3x_(&triangle, &n, a, &lda, const_cast<T*>(off_diagonal), pivots,
                   work, &size, &info, 1);
      } else {
        LAPACK_csytri_3(&triangle, &n, a, &lda, off_diagonal, pivots, work,
                        &size, &info);
      }
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (options.hermitian) {
      if (options.explicit_block) {
        zhetri_3x_(&triangle, &n, a, &lda, const_cast<T*>(off_diagonal), pivots,
                   work, &size, &info, 1);
      } else {
        LAPACK_zhetri_3(&triangle, &n, a, &lda, off_diagonal, pivots, work,
                        &size, &info);
      }
    } else {
      if (options.explicit_block) {
        zsytri_3x_(&triangle, &n, a, &lda, const_cast<T*>(off_diagonal), pivots,
                   work, &size, &info, 1);
      } else {
        LAPACK_zsytri_3(&triangle, &n, a, &lda, off_diagonal, pivots, work,
                        &size, &info);
      }
    }
  }
}

Status PivotMetadata(const ReferenceLapackProvider& provider,
                     RawLapackPivotView pivots, extent_t order) {
  if (pivots.family() != LapackFactorFamily::kRook) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (pivots.values().size() != static_cast<std::size_t>(order)) {
    return Status(ErrorCode::kShape);
  }
  return bk::Accessible(provider, pivots.reachable_storage());
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasMatrixView<T> factors,
                                  DenseBlasVectorView<const T> off_diagonal,
                                  RawLapackPivotView pivots, Options options) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns() ||
      off_diagonal.size() != factors.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (off_diagonal.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array spans{factors.reachable_storage(),
                         off_diagonal.reachable_storage(),
                         pivots.reachable_storage(), bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, factors),
        bk::Accessible(provider, off_diagonal.reachable_storage()),
        PivotMetadata(provider, pivots, factors.rows()), bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto count = internal_indefinite_rk_inverse_counts::Workspace(
      factors.rows(), bk::Leading(factors), options,
      triangle == DenseBlasTriangle::kUpper, bk::kIntegerLimit);
  if (!count.ok()) {
    return count.status();
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(options), bk::ScalarKind<T>(),
      std::array{factors.rows(), factors.columns(), bk::Leading(factors),
                 options.block_size, off_diagonal.size(),
                 static_cast<extent_t>(pivots.values().size())},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(options.hermitian),
          static_cast<std::int64_t>(options.explicit_block),
          static_cast<std::int64_t>(factors.layout()),
          factors.leading_dimension(), off_diagonal.increment(),
          static_cast<std::int64_t>(pivots.family())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (factors.rows() != 0) {
    const auto n = factors.rows();
    extent_t preferred = *count;
    if (!options.explicit_block) {
      const auto value = internal_indefinite_rk_inverse_counts::Preferred<
          DenseBlasRealType<T>>(*count, bk::kIntegerLimit);
      if (!value.ok()) {
        return value.status();
      }
      preferred = *value;
    }
    plan.regions[bk::kScalar] = {*count, preferred, sizeof(T), alignof(T)};
    plan.regions[bk::kPivot] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
    const Status status = bk::Packing(factors, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

// RK uses each pivot's own directional target; old ROOK pair-wide bounds
// describe a different factor representation. Values alone do not prove origin.
Status PivotValues(RawLapackPivotView pivots, extent_t order,
                   DenseBlasTriangle triangle) {
  const bool upper = triangle == DenseBlasTriangle::kUpper;
  const auto values = pivots.values();
  for (extent_t i = 0; i < order;) {
    const index_t p = values[static_cast<std::size_t>(i)];
    if (p == 0 || p < -order || p > order) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const index_t target = p < 0 ? -p : p;
    if ((upper && target > i + 1) || (!upper && target < i + 1)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    if (p > 0) {
      ++i;
      continue;
    }
    if (i + 1 >= order) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const index_t q = values[static_cast<std::size_t>(i + 1)];
    if (q >= 0 || q < -order || (upper && -q > i + 2) ||
        (!upper && -q < i + 2)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    i += 2;
  }
  return Status::Ok();
}

// Mirror only the source's immutable early singular scan. Do not replace
// native inversion or infer a 2x2/finiteness diagnosis absent upstream.
template <typename T>
lapack_int SingularInfo(DenseBlasMatrixView<T> factors,
                        RawLapackPivotView pivots, DenseBlasTriangle triangle) {
  for (extent_t offset = 0; offset < factors.rows(); ++offset) {
    const extent_t i = triangle == DenseBlasTriangle::kUpper
                           ? factors.rows() - 1 - offset
                           : offset;
    if (pivots.values()[static_cast<std::size_t>(i)] > 0 &&
        bk::Entry(factors, i, i) == T{}) {
      return static_cast<lapack_int>(i + 1);
    }
  }
  return 0;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<T> factors,
               DenseBlasVectorView<const T> off_diagonal,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               Options options) {
  const std::array operands{factors.reachable_storage(),
                            off_diagonal.reachable_storage(),
                            pivots.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(options), report);
  report.factor_family = LapackFactorFamily::kRook;
  const auto expected =
      Query(provider, triangle, factors, off_diagonal, pivots, options);
  if (!expected.ok()) {
    return expected.status();
  }
  status = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (factors.rows() == 0) {
    return bk::Complete(report);
  }
  status = PivotValues(pivots, factors.rows(), triangle);
  if (!status.ok()) {
    return status;
  }
  const lapack_int expected_info = SingularInfo(factors, pivots, triangle);
  auto* native_pivots =
      internal_indefinite_expert::PreparePivots(pivots, plan, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* packed = bk::PackTriangle(factors, triangle, false, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  if (!options.explicit_block) {
    if constexpr (DenseBlasComplex<T>) {
      work[0] = T{-1, -1};
    } else {
      work[0] = T{-1};
    }
  }
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  const auto size = static_cast<lapack_int>(
      options.explicit_block ? options.block_size
                             : plan.regions[bk::kScalar].minimum_entries);
  Call(options, bk::Uplo(triangle), static_cast<lapack_int>(factors.rows()),
       packed, static_cast<lapack_int>(bk::Leading(factors)),
       off_diagonal.data(), native_pivots, work, size, info);
  report.native_info = info;
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    if (native_pivots[i] != pivots.values()[i]) {
      return bk::Defect(info, report);
    }
  }
  if (info != expected_info ||
      (!options.explicit_block &&
       work[0] != T{static_cast<DenseBlasRealType<T>>(
                      plan.regions[bk::kScalar].preferred_entries)})) {
    return bk::Defect(info, report);
  }
  // The E copy precedes the singular scan, but A is unchanged on valid
  // positive INFO. Publish the same selected storage in both layouts.
  bk::PublishTriangle(packed, factors, triangle, false);
  if (info > 0) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  return bk::Complete(report);
}

}  // namespace

Result<LapackWorkspacePlan> QuerySytri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> factors,
    DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{false, false, 1});
}
Status Sytri3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasMatrixView<float> factors,
              DenseBlasVectorView<const float> off_diagonal,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{false, false, 1});
}
Result<LapackWorkspacePlan> QuerySytri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{false, false, 1});
}
Status Sytri3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasMatrixView<double> factors,
              DenseBlasVectorView<const double> off_diagonal,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{false, false, 1});
}
Result<LapackWorkspacePlan> QuerySytri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{false, false, 1});
}
Status Sytri3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<float>> factors,
              DenseBlasVectorView<const std::complex<float>> off_diagonal,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{false, false, 1});
}
Result<LapackWorkspacePlan> QuerySytri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{false, false, 1});
}
Status Sytri3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<double>> factors,
              DenseBlasVectorView<const std::complex<double>> off_diagonal,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{false, false, 1});
}
Result<LapackWorkspacePlan> QueryHetri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{true, false, 1});
}
Status Hetri3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<float>> factors,
              DenseBlasVectorView<const std::complex<float>> off_diagonal,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{true, false, 1});
}
Result<LapackWorkspacePlan> QueryHetri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{true, false, 1});
}
Status Hetri3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<double>> factors,
              DenseBlasVectorView<const std::complex<double>> off_diagonal,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{true, false, 1});
}
Result<LapackWorkspacePlan> QuerySytri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<float> factors,
    DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{false, true, block_size});
}
Status Sytri3x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<float> factors,
               DenseBlasVectorView<const float> off_diagonal,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{false, true, block_size});
}
Result<LapackWorkspacePlan> QuerySytri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{false, true, block_size});
}
Status Sytri3x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<double> factors,
               DenseBlasVectorView<const double> off_diagonal,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{false, true, block_size});
}
Result<LapackWorkspacePlan> QuerySytri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{false, true, block_size});
}
Status Sytri3x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<std::complex<float>> factors,
               DenseBlasVectorView<const std::complex<float>> off_diagonal,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{false, true, block_size});
}
Result<LapackWorkspacePlan> QuerySytri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{false, true, block_size});
}
Status Sytri3x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<std::complex<double>> factors,
               DenseBlasVectorView<const std::complex<double>> off_diagonal,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{false, true, block_size});
}
Result<LapackWorkspacePlan> QueryHetri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{true, true, block_size});
}
Status Hetri3x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<std::complex<float>> factors,
               DenseBlasVectorView<const std::complex<float>> off_diagonal,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{true, true, block_size});
}
Result<LapackWorkspacePlan> QueryHetri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, off_diagonal, pivots,
               Options{true, true, block_size});
}
Status Hetri3x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<std::complex<double>> factors,
               DenseBlasVectorView<const std::complex<double>> off_diagonal,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, plan,
                 workspace, report, Options{true, true, block_size});
}

}  // namespace asc
