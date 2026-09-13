#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; declares nonallocating placement new.
#include <span>
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
#include "asc/dense/providers/lapack_lu_condition.h"
#include "internal_workspace_context.h"
#include "lapack_build_config.h"

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#elif ASC_LAPACK_INTEGER_BITS != 32
#error "Reference LAPACK requires an explicitly selected 32/64 integer ABI"
#endif
#include <lapack.h>
#include <lapacke_config.h>

#if !defined(__GLIBCXX__) || __GLIBCXX__ != 20230528
#error "Reference LAPACK complex ABI requires the audited libstdc++ build"
#endif

namespace asc {
namespace {
constexpr auto kScalar = static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr auto kReal = static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);
static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kScalarKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sgecon";
  static lapack_int Execute(char norm, lapack_int n, const float* a,
                            lapack_int lda, float anorm, float& rcond,
                            const LapackWorkspace& workspace) {
    // The provider must write the complete native INFO destination.
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgecon(&norm, &n, a, &lda, &anorm, &rcond,
                  static_cast<float*>(workspace.regions[kScalar].data()),
                  static_cast<lapack_int*>(workspace.regions[kInteger].data()),
                  &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kScalarKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dgecon";
  static lapack_int Execute(char norm, lapack_int n, const double* a,
                            lapack_int lda, double anorm, double& rcond,
                            const LapackWorkspace& workspace) {
    // The provider must write the complete native INFO destination.
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgecon(&norm, &n, a, &lda, &anorm, &rcond,
                  static_cast<double*>(workspace.regions[kScalar].data()),
                  static_cast<lapack_int*>(workspace.regions[kInteger].data()),
                  &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalarKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cgecon";
  static lapack_int Execute(char norm, lapack_int n,
                            const std::complex<float>* a, lapack_int lda,
                            float anorm, float& rcond,
                            const LapackWorkspace& workspace) {
    // The provider must write the complete native INFO destination.
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgecon(
        &norm, &n, a, &lda, &anorm, &rcond,
        static_cast<std::complex<float>*>(workspace.regions[kScalar].data()),
        static_cast<float*>(workspace.regions[kReal].data()), &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalarKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zgecon";
  static lapack_int Execute(char norm, lapack_int n,
                            const std::complex<double>* a, lapack_int lda,
                            double anorm, double& rcond,
                            const LapackWorkspace& workspace) {
    // The provider must write the complete native INFO destination.
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgecon(
        &norm, &n, a, &lda, &anorm, &rcond,
        static_cast<std::complex<double>*>(workspace.regions[kScalar].data()),
        static_cast<double*>(workspace.regions[kReal].data()), &info);
    return info;
  }
};

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  const auto a = reinterpret_cast<std::uintptr_t>(first.data());
  const auto b = reinterpret_cast<std::uintptr_t>(second.data());
  return first.size() != 0 && second.size() != 0 && a < b + second.size() &&
         b < a + first.size();
}

bool TotalFits(const LapackWorkspacePlan& plan) {
  std::size_t remaining = std::numeric_limits<std::size_t>::max();
  for (const auto& region : plan.regions) {
    const auto entries = static_cast<std::uint64_t>(region.minimum_entries);
    if (entries > remaining / region.entry_bytes) {
      return false;
    }
    remaining -= static_cast<std::size_t>(entries) * region.entry_bytes;
  }
  return true;
}

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const T> factors, DenseBlasRealType<T> original_norm,
    const DenseBlasRealType<T>& reciprocal_condition) {
  using Real = DenseBlasRealType<T>;
  if (norm != LapackConditionNorm::kOne &&
      norm != LapackConditionNorm::kInfinity) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (!std::isfinite(original_norm) || original_norm < Real{0}) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if ((factors.memory_space() != MemorySpace::kHost &&
       factors.memory_space() != MemorySpace::kPinnedHost) ||
      !provider.context().CanAccess(factors.memory_space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (factors.rows() != factors.columns()) {
    return Status(ErrorCode::kShape);
  }
  // Complex LACN2 computes 3*N internally despite 2*N WORK/RWORK.
  constexpr extent_t kIntegerMultiplier = DenseBlasComplex<T> ? 3 : 4;
  constexpr extent_t kScalarMultiplier = DenseBlasComplex<T> ? 2 : 4;
  const extent_t n = factors.rows();
  const extent_t foreign_ld = factors.layout() == DenseBlasLayout::kColumnMajor
                                  ? factors.leading_dimension()
                                  : std::max<extent_t>(1, n);
  if (n > std::numeric_limits<lapack_int>::max() / kIntegerMultiplier ||
      foreign_ld > std::numeric_limits<lapack_int>::max()) {
    return Status(ErrorCode::kOverflow);
  }
  if (Overlap(factors.reachable_storage(),
              {&reciprocal_condition, sizeof(Real), MemorySpace::kHost})) {
    return Status(ErrorCode::kInvalidArgument);
  }
  extent_t packed_entries = 0;
  if (factors.layout() == DenseBlasLayout::kRowMajor && n != 0) {
    if (n > std::numeric_limits<extent_t>::max() / n) {
      return Status(ErrorCode::kOverflow);
    }
    packed_entries = n * n;
  }
  const extent_t scalar_entries = kScalarMultiplier * n;
  if (static_cast<std::uint64_t>(packed_entries) >
          std::numeric_limits<std::size_t>::max() / sizeof(T) ||
      static_cast<std::uint64_t>(scalar_entries) >
          std::numeric_limits<std::size_t>::max() / sizeof(T)) {
    return Status(ErrorCode::kOverflow);
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalarKind, std::array{n, foreign_ld},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(norm),
                                  static_cast<std::int64_t>(factors.layout()),
                                  factors.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  plan.regions[kScalar] = {scalar_entries, scalar_entries, sizeof(T),
                           alignof(T)};
  if constexpr (DenseBlasComplex<T>) {
    plan.regions[kReal] = {2 * n, 2 * n, sizeof(Real), alignof(Real)};
  } else {
    if (static_cast<std::uint64_t>(n) >
        std::numeric_limits<std::size_t>::max() / sizeof(lapack_int)) {
      return Status(ErrorCode::kOverflow);
    }
    plan.regions[kInteger] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
  }
  if (factors.layout() == DenseBlasLayout::kRowMajor) {
    plan.regions[kLayout] = {packed_entries, packed_entries, sizeof(T),
                             alignof(T)};
  }
  if (!TotalFits(plan)) {
    return Status(ErrorCode::kOverflow);
  }
  return plan;
}

Status ValidatePlan(const ReferenceLapackProvider& provider,
                    const LapackWorkspacePlan& expected,
                    const LapackWorkspacePlan& supplied,
                    const LapackWorkspace& workspace,
                    std::span<const ConstMemoryView> operands) {
  if (expected.total_byte_limit != supplied.total_byte_limit) {
    return Status(ErrorCode::kInvalidState);
  }
  for (std::size_t i = 0; i < expected.regions.size(); ++i) {
    const auto& a = expected.regions[i];
    const auto& b = supplied.regions[i];
    if (a.minimum_entries != b.minimum_entries ||
        a.preferred_entries != b.preferred_entries ||
        a.entry_bytes != b.entry_bytes || a.alignment != b.alignment) {
      return Status(ErrorCode::kInvalidState);
    }
  }
  return internal_lapack_workspace::Validate(
      provider, supplied, expected.identity, workspace, operands);
}

template <typename T>
Status Estimate(const ReferenceLapackProvider& provider,
                LapackConditionNorm norm, DenseBlasMatrixView<const T> factors,
                DenseBlasRealType<T> original_norm,
                DenseBlasRealType<T>& reciprocal_condition,
                const LapackWorkspacePlan& plan,
                const LapackWorkspace& workspace, LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  const auto name = Native<T>::kName;
  std::copy(name.begin(), name.end(), report.routine.begin());
  const auto expected =
      Query(provider, norm, factors, original_norm, reciprocal_condition);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status =
      ValidatePlan(provider, *expected, plan, workspace,
                   std::array{factors.reachable_storage(),
                              ConstMemoryView(&reciprocal_condition,
                                              sizeof(reciprocal_condition),
                                              MemorySpace::kHost)});
  if (!status.ok()) {
    return status;
  }
  if (factors.rows() == 0) {
    reciprocal_condition = 1;
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  const T* data = factors.data();
  extent_t ld = factors.leading_dimension();
  if (factors.layout() == DenseBlasLayout::kRowMajor) {
    auto* packed = static_cast<T*>(workspace.regions[kLayout].data());
    for (extent_t j = 0; j < factors.rows(); ++j) {
      for (extent_t i = 0; i < factors.rows(); ++i) {
        packed[j * factors.rows() + i] = data[i * ld + j];
      }
    }
    data = packed;
    ld = factors.rows();
  }
  if constexpr (!DenseBlasComplex<T>) {
    auto* integers =
        static_cast<lapack_int*>(workspace.regions[kInteger].data());
    for (extent_t i = 0; i < factors.rows(); ++i) {
      ::new (static_cast<void*>(integers + i)) lapack_int;
    }
  }
  report.called_provider = true;
  const lapack_int info =
      Native<T>::Execute(norm == LapackConditionNorm::kOne ? '1' : 'I',
                         static_cast<lapack_int>(factors.rows()), data,
                         static_cast<lapack_int>(ld), original_norm,
                         reciprocal_condition, workspace);
  report.native_info = info;
  if (info < 0) {
    report.outcome = LapackOutcome::kProviderArgument;
    report.output_validity = LapackOutputValidity::kUnusable;
    if (info != std::numeric_limits<lapack_int>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  if (info > 1) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  if (info == 1) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}
}  // namespace

Result<LapackWorkspacePlan> QueryGeconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const float> factors, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, norm, factors, original_norm, reciprocal_condition);
}
Status Gecon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             DenseBlasMatrixView<const float> factors, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Estimate(provider, norm, factors, original_norm, reciprocal_condition,
                  plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const double> factors, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, norm, factors, original_norm, reciprocal_condition);
}
Status Gecon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             DenseBlasMatrixView<const double> factors, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Estimate(provider, norm, factors, original_norm, reciprocal_condition,
                  plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const std::complex<float>> factors, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, norm, factors, original_norm, reciprocal_condition);
}
Status Gecon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             DenseBlasMatrixView<const std::complex<float>> factors,
             float original_norm, float& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Estimate(provider, norm, factors, original_norm, reciprocal_condition,
                  plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const std::complex<double>> factors,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, norm, factors, original_norm, reciprocal_condition);
}
Status Gecon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             DenseBlasMatrixView<const std::complex<double>> factors,
             double original_norm, double& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Estimate(provider, norm, factors, original_norm, reciprocal_condition,
                  plan, workspace, report);
}

}  // namespace asc
