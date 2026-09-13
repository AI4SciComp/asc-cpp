#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band_refinement.h"
#include "internal_indefinite.h"
#include "internal_layout.h"
#include "internal_lu_band_expert.h"
#include "internal_lu_band_expert_counts.h"
namespace asc {
namespace {
namespace checked = internal_lu_band_expert;
namespace common = internal_indefinite;
namespace counts = internal_lu_band_expert_counts;
namespace layout = internal_lapack_layout;
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sgbrfs";
  static constexpr auto kExecute = LAPACK_sgbrfs_base;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dgbrfs";
  static constexpr auto kExecute = LAPACK_dgbrfs_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cgbrfs";
  static constexpr auto kExecute = LAPACK_cgbrfs_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zgbrfs";
  static constexpr auto kExecute = LAPACK_zgbrfs_base;
};
template <typename T>
struct Operands {
  ReferenceGeneralBandView<const T> a;
  LapackLuBandView<const T> af;
  ReferenceLuBandPivotView pivots;
  DenseBlasMatrixView<const T> b;
  DenseBlasMatrixView<T> x;
  DenseBlasVectorView<DenseBlasRealType<T>> ferr;
  DenseBlasVectorView<DenseBlasRealType<T>> berr;
  [[nodiscard]] auto Spans() const {
    return std::array{
        a.storage().reachable_storage(), af.storage().reachable_storage(),
        pivots.reachable_storage(),      b.reachable_storage(),
        x.reachable_storage(),           ferr.reachable_storage(),
        berr.reachable_storage()};
  }
};
template <typename T>
Status Structure(const ReferenceLapackProvider& provider,
                 DenseBlasTranspose transpose, const Operands<T>& data) {
  const auto n = data.a.rows();
  if (!checked::Operation(transpose)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (data.a.columns() != n || data.af.rows() != n || data.af.columns() != n ||
      data.af.lower_bandwidth() != data.a.lower_bandwidth() ||
      data.af.upper_bandwidth() != data.a.upper_bandwidth() ||
      data.b.rows() != n || data.x.rows() != n ||
      data.b.columns() != data.x.columns()) {
    return Status(ErrorCode::kShape);
  }
  for (const auto span : data.Spans()) {
    auto status = common::Accessible(provider, span);
    if (!status.ok()) {
      return status;
    }
    if (common::Overlap(span, common::Object(provider))) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  for (const auto& status :
       {checked::Vector(provider, data.pivots.storage(), n),
        checked::Vector(provider, data.ferr, data.b.columns()),
        checked::Vector(provider, data.berr, data.b.columns()),
        common::Disjoint(data.Spans()),
        counts::Refine(n, data.a.lower_bandwidth(), data.a.upper_bandwidth(),
                       data.a.storage().leading_dimension(),
                       data.af.storage().leading_dimension(), data.b.columns(),
                       checked::RhsLeading(data.b), checked::RhsLeading(data.x),
                       common::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTranspose transpose,
                                  const Operands<T>& data) {
  auto status = Structure(provider, transpose, data);
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{data.a.rows(), data.a.lower_bandwidth(),
                 data.a.upper_bandwidth(), data.a.storage().leading_dimension(),
                 data.af.storage().leading_dimension(), data.b.columns(),
                 checked::RhsLeading(data.b), checked::RhsLeading(data.x)},
      std::array<std::int64_t, 5>{static_cast<std::int64_t>(transpose),
                                  static_cast<std::int64_t>(data.b.layout()),
                                  data.b.leading_dimension(),
                                  static_cast<std::int64_t>(data.x.layout()),
                                  data.x.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  for (const auto& checked_status :
       {checked::EstimateWorkspace<T>(data.a.rows(), plan),
        layout::AddPacking(data.b, plan), layout::AddPacking(data.x, plan)}) {
    if (!checked_status.ok()) {
      return checked_status;
    }
  }
  return plan;
}
template <typename T>
lapack_int Call(DenseBlasTranspose transpose, const Operands<T>& data,
                const T* b, T* x, lapack_int* integers,
                const LapackWorkspace& workspace) {
  using Real = DenseBlasRealType<T>;
  const char trans = checked::Transpose(transpose);
  const auto n = static_cast<lapack_int>(data.a.rows());
  const auto kl = static_cast<lapack_int>(data.a.lower_bandwidth());
  const auto ku = static_cast<lapack_int>(data.a.upper_bandwidth());
  const auto nrhs = static_cast<lapack_int>(data.b.columns());
  const auto ld = static_cast<lapack_int>(data.a.storage().leading_dimension());
  const auto ldf =
      static_cast<lapack_int>(data.af.storage().leading_dimension());
  const auto ldb = static_cast<lapack_int>(checked::RhsLeading(data.b));
  const auto ldx = static_cast<lapack_int>(checked::RhsLeading(data.x));
  const T input_dummy{};
  T scalar_dummy{};
  Real real_dummy{};
  auto* work = checked::Nonnull(
      static_cast<T*>(workspace.regions[common::kScalar].data()), scalar_dummy);
  auto* ferr = checked::Nonnull(data.ferr.data(), real_dummy);
  auto* berr = checked::Nonnull(data.berr.data(), real_dummy);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (DenseBlasComplex<T>) {
    auto* real = checked::Nonnull(
        static_cast<Real*>(workspace.regions[checked::kReal].data()),
        real_dummy);
    Native<T>::kExecute(&trans, &n, &kl, &ku, &nrhs,
                        checked::Nonnull(data.a.storage().data(), input_dummy),
                        &ld,
                        checked::Nonnull(data.af.storage().data(), input_dummy),
                        &ldf, integers, checked::Nonnull(b, input_dummy), &ldb,
                        checked::Nonnull(x, scalar_dummy), &ldx, ferr, berr,
                        work, real, &info, std::size_t{1});
  } else {
    Native<T>::kExecute(&trans, &n, &kl, &ku, &nrhs,
                        checked::Nonnull(data.a.storage().data(), input_dummy),
                        &ld,
                        checked::Nonnull(data.af.storage().data(), input_dummy),
                        &ldf, integers, checked::Nonnull(b, input_dummy), &ldb,
                        checked::Nonnull(x, scalar_dummy), &ldx, ferr, berr,
                        work, integers + n, &info, std::size_t{1});
  }
  return info;
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTranspose transpose, const Operands<T>& data,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  auto status =
      common::Metadata(provider, plan, workspace, report, data.Spans());
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kName, report);
  const auto expected = Query(provider, transpose, data);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, data.Spans());
  if (!status.ok()) {
    return status;
  }
  status = checked::Pivots(data.pivots.values(), data.a.rows(),
                           data.a.lower_bandwidth());
  if (!status.ok()) {
    return status;
  }
  if (data.a.rows() != 0 && data.b.columns() != 0) {
    status = checked::ZeroDiagonal(data.af, report);
    if (!status.ok()) {
      return status;
    }
  }
  lapack_int dummy = std::numeric_limits<lapack_int>::min();
  auto* integers = checked::Integers(
      workspace, plan.regions[common::kPivot].minimum_entries, dummy);
  std::copy(data.pivots.values().begin(), data.pivots.values().end(), integers);
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  const T* b = layout::Pack(data.b, cursor);
  T* x = layout::Pack(data.x, cursor);
  report.called_provider = true;
  const auto info = Call(transpose, data, b, x, integers, workspace);
  report.native_info = info;
  if (info != 0) {
    return common::Defect(info, report);
  }
  layout::Unpack(x, data.x);
  return checked::Errors(data.ferr, data.berr, report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryGbrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider, transpose,
               Operands<float>{original, factors, pivots, rhs, solution,
                               forward_error, backward_error});
}
Status Gbrfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, transpose,
                 Operands<float>{original, factors, pivots, rhs, solution,
                                 forward_error, backward_error},
                 plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider, transpose,
               Operands<double>{original, factors, pivots, rhs, solution,
                                forward_error, backward_error});
}
Status Gbrfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, transpose,
                 Operands<double>{original, factors, pivots, rhs, solution,
                                  forward_error, backward_error},
                 plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<float>> original,
    LapackLuBandView<const std::complex<float>> factors,
    ReferenceLuBandPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(
      provider, transpose,
      Operands<std::complex<float>>{original, factors, pivots, rhs, solution,
                                    forward_error, backward_error});
}
Status Gbrfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceGeneralBandView<const std::complex<float>> original,
             LapackLuBandView<const std::complex<float>> factors,
             ReferenceLuBandPivotView pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider, transpose,
      Operands<std::complex<float>>{original, factors, pivots, rhs, solution,
                                    forward_error, backward_error},
      plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<double>> original,
    LapackLuBandView<const std::complex<double>> factors,
    ReferenceLuBandPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(
      provider, transpose,
      Operands<std::complex<double>>{original, factors, pivots, rhs, solution,
                                     forward_error, backward_error});
}
Status Gbrfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceGeneralBandView<const std::complex<double>> original,
             LapackLuBandView<const std::complex<double>> factors,
             ReferenceLuBandPivotView pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider, transpose,
      Operands<std::complex<double>>{original, factors, pivots, rhs, solution,
                                     forward_error, backward_error},
      plan, workspace, report);
}
}  // namespace asc
