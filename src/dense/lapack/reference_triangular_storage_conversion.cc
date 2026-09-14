#include <algorithm>
#include <array>
#include <complex>
#include <cstdint>
#include <limits>
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
#include "asc/dense/providers/lapack_triangular_storage_conversion.h"
#include "internal_layout.h"
#include "internal_packed_triangular.h"
#include "internal_triangular_storage_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace layout = internal_lapack_layout;
namespace packed = internal_packed_triangular;

template <bool ToPacked, typename T>
using Matrix = DenseBlasMatrixView<std::conditional_t<ToPacked, const T, T>>;
template <bool ToPacked, typename T>
using Packed =
    DenseBlasPackedMatrixView<std::conditional_t<ToPacked, T, const T>>;

template <bool ToPacked, typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return ToPacked ? "strttp" : "stpttr";
  } else if constexpr (std::is_same_v<T, double>) {
    return ToPacked ? "dtrttp" : "dtpttr";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return ToPacked ? "ctrttp" : "ctpttr";
  } else {
    return ToPacked ? "ztrttp" : "ztpttr";
  }
}

template <bool ToPacked, typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  Matrix<ToPacked, T> matrix,
                                  Packed<ToPacked, T> triangle_storage) {
  if (triangle != DenseBlasTriangle::kUpper &&
      triangle != DenseBlasTriangle::kLower) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const extent_t n = triangle_storage.order();
  if (matrix.rows() != n || matrix.columns() != n) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{ConstMemoryView(matrix.reachable_storage()),
                         triangle_storage.reachable_storage()};
  Status status = checked::Disjoint(spans);
  for (const auto storage : spans) {
    if (status.ok()) {
      status = checked::Access(provider, storage);
    }
  }
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      Routine<ToPacked, T>(), checked::Kind<T>(),
      std::array{n, matrix.leading_dimension()},
      std::array<std::int64_t, 3>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(matrix.layout()),
          static_cast<std::int64_t>(triangle_storage.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (n == 0) {
    return plan;
  }
  const extent_t native_leading = ToPacked ? checked::Leading(matrix) : n;
  const auto counts = internal_triangular_storage_counts::Count(
      n, native_leading, ToPacked, checked::kLimit);
  if (!counts.ok()) {
    return counts.status();
  }
  const auto staged = ToPacked ? counts->packed : counts->dense;
  plan.regions[checked::kScratch] = {staged, staged, sizeof(T), alignof(T)};
  const auto input_layout =
      ToPacked ? matrix.layout() : triangle_storage.layout();
  if (input_layout == DenseBlasLayout::kRowMajor) {
    const auto converted = ToPacked ? counts->dense : counts->packed;
    plan.regions[checked::kLayout] = {converted, converted, sizeof(T),
                                      alignof(T)};
  }
  status = layout::CheckTotal(plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename Function>
void EachEntry(extent_t n, DenseBlasTriangle triangle, Function&& function) {
  for (extent_t j = 0; j < n; ++j) {
    const auto first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto last = triangle == DenseBlasTriangle::kUpper ? j + 1 : n;
    for (extent_t i = first; i < last; ++i) {
      function(i, j);
    }
  }
}

template <typename T>
extent_t DenseOffset(DenseBlasMatrixView<T> matrix, extent_t i, extent_t j) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? j * matrix.leading_dimension() + i
             : i * matrix.leading_dimension() + j;
}

template <bool ToPacked, typename T>
const T* Input(Matrix<ToPacked, T> matrix, Packed<ToPacked, T> triangle_storage,
               DenseBlasTriangle triangle, const LapackWorkspace& workspace) {
  const extent_t n = triangle_storage.order();
  const auto input_layout =
      ToPacked ? matrix.layout() : triangle_storage.layout();
  if (input_layout == DenseBlasLayout::kColumnMajor) {
    if constexpr (ToPacked) {
      return matrix.data();
    } else {
      return triangle_storage.data();
    }
  }
  auto* converted = static_cast<T*>(workspace.regions[checked::kLayout].data());
  EachEntry(n, triangle, [&](extent_t i, extent_t j) {
    if constexpr (ToPacked) {
      converted[j * n + i] = matrix.data()[DenseOffset(matrix, i, j)];
    } else {
      converted[packed::Offset(n, triangle, DenseBlasLayout::kColumnMajor, i,
                               j)] =
          triangle_storage.data()[packed::Offset(
              n, triangle, triangle_storage.layout(), i, j)];
    }
  });
  return converted;
}

template <bool ToPacked, typename T>
lapack_int Native(char uplo, lapack_int n, const T* input, lapack_int leading,
                  T* output) {
  lapack_int info = std::numeric_limits<lapack_int>::max();
  if constexpr (std::is_same_v<T, float>) {
    if constexpr (ToPacked) {
      LAPACK_strttp(&uplo, &n, input, &leading, output, &info);
    } else {
      LAPACK_stpttr(&uplo, &n, input, output, &leading, &info);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if constexpr (ToPacked) {
      LAPACK_dtrttp(&uplo, &n, input, &leading, output, &info);
    } else {
      LAPACK_dtpttr(&uplo, &n, input, output, &leading, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if constexpr (ToPacked) {
      LAPACK_ctrttp(&uplo, &n, input, &leading, output, &info);
    } else {
      LAPACK_ctpttr(&uplo, &n, input, output, &leading, &info);
    }
  } else {
    if constexpr (ToPacked) {
      LAPACK_ztrttp(&uplo, &n, input, &leading, output, &info);
    } else {
      LAPACK_ztpttr(&uplo, &n, input, output, &leading, &info);
    }
  }
  return info;
}

template <bool ToPacked, typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, Matrix<ToPacked, T> matrix,
               Packed<ToPacked, T> triangle_storage,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array spans{ConstMemoryView(matrix.reachable_storage()),
                         triangle_storage.reachable_storage()};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<ToPacked, T>(), report);
  const auto expected =
      Query<ToPacked, T>(provider, triangle, matrix, triangle_storage);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  const extent_t n = triangle_storage.order();
  if (n == 0) {
    return checked::Complete(report);
  }
  const T* input =
      Input<ToPacked, T>(matrix, triangle_storage, triangle, workspace);
  auto* staged = static_cast<T*>(workspace.regions[checked::kScratch].data());
  // Initialize live scratch without reading old output. No omitted-write
  // certificate is inferred from these values or from native INFO zero.
  std::fill_n(staged, plan.regions[checked::kScratch].minimum_entries, T{});
  const auto leading = ToPacked ? checked::Leading(matrix) : n;
  const char uplo = triangle == DenseBlasTriangle::kUpper ? 'U' : 'L';
  report.called_provider = true;
  const auto info = Native<ToPacked>(uplo, static_cast<lapack_int>(n), input,
                                     static_cast<lapack_int>(leading), staged);
  report.native_info = info;
  if (info != 0) {
    return checked::ProviderDefect(info, report);
  }
  EachEntry(n, triangle, [&](extent_t i, extent_t j) {
    if constexpr (ToPacked) {
      triangle_storage.data()[packed::Offset(n, triangle,
                                             triangle_storage.layout(), i, j)] =
          staged[packed::Offset(n, triangle, DenseBlasLayout::kColumnMajor, i,
                                j)];
    } else {
      matrix.data()[DenseOffset(matrix, i, j)] = staged[j * n + i];
    }
  });
  return checked::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryTrttpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> input,
    DenseBlasPackedMatrixView<float> output) {
  return Query<true, float>(provider, triangle, input, output);
}
Status Trttp(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<const float> input,
             DenseBlasPackedMatrixView<float> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<true, float>(provider, triangle, input, output, plan,
                              workspace, report);
}
Result<LapackWorkspacePlan> QueryTrttpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> input,
    DenseBlasPackedMatrixView<double> output) {
  return Query<true, double>(provider, triangle, input, output);
}
Status Trttp(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const double> input,
             DenseBlasPackedMatrixView<double> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<true, double>(provider, triangle, input, output, plan,
                               workspace, report);
}
Result<LapackWorkspacePlan> QueryTrttpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> input,
    DenseBlasPackedMatrixView<std::complex<float>> output) {
  return Query<true, std::complex<float>>(provider, triangle, input, output);
}
Status Trttp(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<float>> input,
             DenseBlasPackedMatrixView<std::complex<float>> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<true, std::complex<float>>(provider, triangle, input, output,
                                            plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryTrttpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> input,
    DenseBlasPackedMatrixView<std::complex<double>> output) {
  return Query<true, std::complex<double>>(provider, triangle, input, output);
}
Status Trttp(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<double>> input,
             DenseBlasPackedMatrixView<std::complex<double>> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<true, std::complex<double>>(provider, triangle, input, output,
                                             plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryTpttrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> input,
    DenseBlasMatrixView<float> output) {
  return Query<false, float>(provider, triangle, output, input);
}
Status Tpttr(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const float> input,
             DenseBlasMatrixView<float> output, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute<false, float>(provider, triangle, output, input, plan,
                               workspace, report);
}
Result<LapackWorkspacePlan> QueryTpttrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> input,
    DenseBlasMatrixView<double> output) {
  return Query<false, double>(provider, triangle, output, input);
}
Status Tpttr(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const double> input,
             DenseBlasMatrixView<double> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<false, double>(provider, triangle, output, input, plan,
                                workspace, report);
}
Result<LapackWorkspacePlan> QueryTpttrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> input,
    DenseBlasMatrixView<std::complex<float>> output) {
  return Query<false, std::complex<float>>(provider, triangle, output, input);
}
Status Tpttr(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> input,
             DenseBlasMatrixView<std::complex<float>> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<false, std::complex<float>>(provider, triangle, output, input,
                                             plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryTpttrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> input,
    DenseBlasMatrixView<std::complex<double>> output) {
  return Query<false, std::complex<double>>(provider, triangle, output, input);
}
Status Tpttr(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> input,
             DenseBlasMatrixView<std::complex<double>> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<false, std::complex<double>>(provider, triangle, output, input,
                                              plan, workspace, report);
}
}  // namespace asc
