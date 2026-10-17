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
#include "asc/dense/providers/lapack_indefinite_block_inverse.h"
#include "internal_indefinite.h"
#include "internal_indefinite_block_inverse_counts.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_expert.h"

namespace asc {
namespace {
namespace bk = internal_indefinite;
using internal_indefinite_block_inverse_counts::Options;

template <typename T>
extent_t DefaultBlock(bool hermitian) {
  return hermitian || std::is_same_v<T, float> ? 64 : 1;
}
template <typename T>
std::string_view Name(Options options) {
  if constexpr (std::is_same_v<T, float>) {
    return options.explicit_block ? "ssytri2x" : "ssytri2";
  } else if constexpr (std::is_same_v<T, double>) {
    return options.explicit_block ? "dsytri2x" : "dsytri2";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (options.hermitian) {
      return options.explicit_block ? "chetri2x" : "chetri2";
    }
    return options.explicit_block ? "csytri2x" : "csytri2";
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (options.hermitian) {
      return options.explicit_block ? "zhetri2x" : "zhetri2";
    }
    return options.explicit_block ? "zsytri2x" : "zsytri2";
  }
}
template <typename T>
void Call(Options options, char triangle, lapack_int n, T* a, lapack_int lda,
          const lapack_int* pivots, T* work, lapack_int size,
          lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    if (options.explicit_block) {
      LAPACK_ssytri2x(&triangle, &n, a, &lda, pivots, work, &size, &info);
    } else {
      LAPACK_ssytri2(&triangle, &n, a, &lda, pivots, work, &size, &info);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (options.explicit_block) {
      LAPACK_dsytri2x(&triangle, &n, a, &lda, pivots, work, &size, &info);
    } else {
      LAPACK_dsytri2(&triangle, &n, a, &lda, pivots, work, &size, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (options.hermitian) {
      if (options.explicit_block) {
        LAPACK_chetri2x(&triangle, &n, a, &lda, pivots, work, &size, &info);
      } else {
        LAPACK_chetri2(&triangle, &n, a, &lda, pivots, work, &size, &info);
      }
    } else {
      if (options.explicit_block) {
        LAPACK_csytri2x(&triangle, &n, a, &lda, pivots, work, &size, &info);
      } else {
        LAPACK_csytri2(&triangle, &n, a, &lda, pivots, work, &size, &info);
      }
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (options.hermitian) {
      if (options.explicit_block) {
        LAPACK_zhetri2x(&triangle, &n, a, &lda, pivots, work, &size, &info);
      } else {
        LAPACK_zhetri2(&triangle, &n, a, &lda, pivots, work, &size, &info);
      }
    } else {
      if (options.explicit_block) {
        LAPACK_zsytri2x(&triangle, &n, a, &lda, pivots, work, &size, &info);
      } else {
        LAPACK_zsytri2(&triangle, &n, a, &lda, pivots, work, &size, &info);
      }
    }
  }
}

Status PivotMetadata(const ReferenceLapackProvider& provider,
                     RawLapackPivotView pivots, extent_t order) {
  if (pivots.family() != LapackFactorFamily::kBunchKaufman) {
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
                                  RawLapackPivotView pivots, Options options) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns()) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{factors.reachable_storage(),
                         pivots.reachable_storage(), bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, factors),
        PivotMetadata(provider, pivots, factors.rows()), bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto count = internal_indefinite_block_inverse_counts::Workspace(
      factors.rows(), bk::Leading(factors), options,
      triangle == DenseBlasTriangle::kUpper, bk::kIntegerLimit);
  if (!count.ok()) {
    return count.status();
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(options), bk::ScalarKind<T>(),
      std::array{factors.rows(), factors.columns(), bk::Leading(factors),
                 options.block_size},
      std::array<std::int64_t, 5>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(options.hermitian),
          static_cast<std::int64_t>(options.explicit_block),
          static_cast<std::int64_t>(factors.layout()),
          factors.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (factors.rows() != 0) {
    const auto n = factors.rows();
    plan.regions[bk::kScalar] = {*count, *count, sizeof(T), alignof(T)};
    plan.regions[bk::kPivot] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
    const Status status = bk::Packing(factors, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
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
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               Options options) {
  const std::array operands{factors.reachable_storage(),
                            pivots.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(options), report);
  const auto expected = Query(provider, triangle, factors, pivots, options);
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
  status = bk::Paired(pivots.values(), factors.rows(), triangle);
  if (!status.ok()) {
    return status;
  }
  const lapack_int expected_info = SingularInfo(factors, pivots, triangle);
  auto* native_pivots =
      internal_indefinite_expert::PreparePivots(pivots, plan, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* packed = bk::PackTriangle(factors, triangle, false, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  const auto size = static_cast<lapack_int>(
      options.explicit_block ? options.block_size
                             : plan.regions[bk::kScalar].minimum_entries);
  Call(options, bk::Uplo(triangle), static_cast<lapack_int>(factors.rows()),
       packed, static_cast<lapack_int>(bk::Leading(factors)), native_pivots,
       work, size, info);
  report.native_info = info;
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    if (native_pivots[i] != pivots.values()[i]) {
      return bk::Defect(info, report);
    }
  }
  if (info != expected_info) {
    return bk::Defect(info, report);
  }
  // SYCONV(C) precedes the TRI2X singular scan. Publish valid native partial
  // output in both layouts; it may no longer represent the original factors.
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

Result<LapackWorkspacePlan> QuerySytri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> factors, RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{false, false, DefaultBlock<float>(false)});
}
Status Sytri2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasMatrixView<float> factors,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{false, false, DefaultBlock<float>(false)});
}

Result<LapackWorkspacePlan> QuerySytri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> factors, RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{false, false, DefaultBlock<double>(false)});
}
Status Sytri2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasMatrixView<double> factors,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{false, false, DefaultBlock<double>(false)});
}

Result<LapackWorkspacePlan> QuerySytri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{false, false, DefaultBlock<std::complex<float>>(false)});
}
Status Sytri2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<float>> factors,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider, triangle, factors, pivots, plan, workspace, report,
      Options{false, false, DefaultBlock<std::complex<float>>(false)});
}

Result<LapackWorkspacePlan> QuerySytri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots) {
  return Query(
      provider, triangle, factors, pivots,
      Options{false, false, DefaultBlock<std::complex<double>>(false)});
}
Status Sytri2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<double>> factors,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider, triangle, factors, pivots, plan, workspace, report,
      Options{false, false, DefaultBlock<std::complex<double>>(false)});
}

Result<LapackWorkspacePlan> QueryHetri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{true, false, DefaultBlock<std::complex<float>>(true)});
}
Status Hetri2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<float>> factors,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{true, false, DefaultBlock<std::complex<float>>(true)});
}

Result<LapackWorkspacePlan> QueryHetri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{true, false, DefaultBlock<std::complex<double>>(true)});
}
Status Hetri2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<double>> factors,
              RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider, triangle, factors, pivots, plan, workspace, report,
      Options{true, false, DefaultBlock<std::complex<double>>(true)});
}

Result<LapackWorkspacePlan> QuerySytri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<float> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{false, true, block_size});
}
Status Sytri2x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<float> factors, RawLapackPivotView pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{false, true, block_size});
}

Result<LapackWorkspacePlan> QuerySytri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<double> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{false, true, block_size});
}
Status Sytri2x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<double> factors, RawLapackPivotView pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{false, true, block_size});
}

Result<LapackWorkspacePlan> QuerySytri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{false, true, block_size});
}
Status Sytri2x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<std::complex<float>> factors,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{false, true, block_size});
}

Result<LapackWorkspacePlan> QuerySytri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{false, true, block_size});
}
Status Sytri2x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<std::complex<double>> factors,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{false, true, block_size});
}

Result<LapackWorkspacePlan> QueryHetri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{true, true, block_size});
}
Status Hetri2x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<std::complex<float>> factors,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{true, true, block_size});
}

Result<LapackWorkspacePlan> QueryHetri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots,
               Options{true, true, block_size});
}
Status Hetri2x(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, extent_t block_size,
               DenseBlasMatrixView<std::complex<double>> factors,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 Options{true, true, block_size});
}

}  // namespace asc
