#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating placement array construction.
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu.h"
#include "internal_layout.h"
#include "lapack_build_config.h"

// Audited GNU boundary; authoritative pinned declarations, not LAPACKE
// wrappers.
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
constexpr std::size_t kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr std::size_t kScalar =
    static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
// Pinned SRC/ilaenv.f, ISPEC=1, C2='GE', C3='TRI', both real/complex.
constexpr extent_t kGetriBlockSize = 64;
static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);

enum class FactorAlgorithm : std::uint8_t { kRecursive, kUnblocked };

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kRecursive = "sgetrf2";
  static constexpr std::string_view kUnblocked = "sgetf2";
  static constexpr std::string_view kInverse = "sgetri";
  static constexpr std::string_view kQueryInverse = "sgetri.query";
  static constexpr std::string_view kDriver = "sgesv";
  static lapack_int Factor(FactorAlgorithm algorithm, lapack_int m,
                           lapack_int n, float* a, lapack_int lda,
                           lapack_int* pivots) {
    lapack_int info = 0;
    if (algorithm == FactorAlgorithm::kRecursive) {
      LAPACK_sgetrf2(&m, &n, a, &lda, pivots, &info);
    } else {
      LAPACK_sgetf2(&m, &n, a, &lda, pivots, &info);
    }
    return info;
  }
  static lapack_int Inverse(lapack_int n, float* a, lapack_int lda,
                            const lapack_int* pivots, float* work,
                            lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_sgetri(&n, a, &lda, pivots, work, &lwork, &info);
    return info;
  }
  static lapack_int Driver(lapack_int n, lapack_int nrhs, float* a,
                           lapack_int lda, lapack_int* pivots, float* b,
                           lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_sgesv(&n, &nrhs, a, &lda, pivots, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kRecursive = "dgetrf2";
  static constexpr std::string_view kUnblocked = "dgetf2";
  static constexpr std::string_view kInverse = "dgetri";
  static constexpr std::string_view kQueryInverse = "dgetri.query";
  static constexpr std::string_view kDriver = "dgesv";
  static lapack_int Factor(FactorAlgorithm algorithm, lapack_int m,
                           lapack_int n, double* a, lapack_int lda,
                           lapack_int* pivots) {
    lapack_int info = 0;
    if (algorithm == FactorAlgorithm::kRecursive) {
      LAPACK_dgetrf2(&m, &n, a, &lda, pivots, &info);
    } else {
      LAPACK_dgetf2(&m, &n, a, &lda, pivots, &info);
    }
    return info;
  }
  static lapack_int Inverse(lapack_int n, double* a, lapack_int lda,
                            const lapack_int* pivots, double* work,
                            lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_dgetri(&n, a, &lda, pivots, work, &lwork, &info);
    return info;
  }
  static lapack_int Driver(lapack_int n, lapack_int nrhs, double* a,
                           lapack_int lda, lapack_int* pivots, double* b,
                           lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_dgesv(&n, &nrhs, a, &lda, pivots, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kRecursive = "cgetrf2";
  static constexpr std::string_view kUnblocked = "cgetf2";
  static constexpr std::string_view kInverse = "cgetri";
  static constexpr std::string_view kQueryInverse = "cgetri.query";
  static constexpr std::string_view kDriver = "cgesv";
  static lapack_int Factor(FactorAlgorithm algorithm, lapack_int m,
                           lapack_int n, std::complex<float>* a, lapack_int lda,
                           lapack_int* pivots) {
    lapack_int info = 0;
    if (algorithm == FactorAlgorithm::kRecursive) {
      LAPACK_cgetrf2(&m, &n, a, &lda, pivots, &info);
    } else {
      LAPACK_cgetf2(&m, &n, a, &lda, pivots, &info);
    }
    return info;
  }
  static lapack_int Inverse(lapack_int n, std::complex<float>* a,
                            lapack_int lda, const lapack_int* pivots,
                            std::complex<float>* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_cgetri(&n, a, &lda, pivots, work, &lwork, &info);
    return info;
  }
  static lapack_int Driver(lapack_int n, lapack_int nrhs,
                           std::complex<float>* a, lapack_int lda,
                           lapack_int* pivots, std::complex<float>* b,
                           lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_cgesv(&n, &nrhs, a, &lda, pivots, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kRecursive = "zgetrf2";
  static constexpr std::string_view kUnblocked = "zgetf2";
  static constexpr std::string_view kInverse = "zgetri";
  static constexpr std::string_view kQueryInverse = "zgetri.query";
  static constexpr std::string_view kDriver = "zgesv";
  static lapack_int Factor(FactorAlgorithm algorithm, lapack_int m,
                           lapack_int n, std::complex<double>* a,
                           lapack_int lda, lapack_int* pivots) {
    lapack_int info = 0;
    if (algorithm == FactorAlgorithm::kRecursive) {
      LAPACK_zgetrf2(&m, &n, a, &lda, pivots, &info);
    } else {
      LAPACK_zgetf2(&m, &n, a, &lda, pivots, &info);
    }
    return info;
  }
  static lapack_int Inverse(lapack_int n, std::complex<double>* a,
                            lapack_int lda, const lapack_int* pivots,
                            std::complex<double>* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_zgetri(&n, a, &lda, pivots, work, &lwork, &info);
    return info;
  }
  static lapack_int Driver(lapack_int n, lapack_int nrhs,
                           std::complex<double>* a, lapack_int lda,
                           lapack_int* pivots, std::complex<double>* b,
                           lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_zgesv(&n, &nrhs, a, &lda, pivots, b, &ldb, &info);
    return info;
  }
};

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  const auto a = reinterpret_cast<std::uintptr_t>(first.data());
  const auto b = reinterpret_cast<std::uintptr_t>(second.data());
  return first.size() != 0 && second.size() != 0 && a < b + second.size() &&
         b < a + first.size();
}

template <typename T>
Status ValidateMatrix(const ReferenceLapackProvider& provider,
                      DenseBlasMatrixView<T> matrix) {
  if ((matrix.memory_space() != MemorySpace::kHost &&
       matrix.memory_space() != MemorySpace::kPinnedHost) ||
      !provider.context().CanAccess(matrix.memory_space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  for (extent_t value : {matrix.rows(), matrix.columns(),
                         internal_lapack_layout::LeadingDimension(matrix)}) {
    if (value > std::numeric_limits<lapack_int>::max()) {
      return Status(ErrorCode::kOverflow);
    }
  }
  return Status::Ok();
}

void StartReport(const ReferenceLapackProvider& provider, std::string_view name,
                 LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  std::copy(name.begin(), name.end(), report.routine.begin());
}

Status Complete(LapackReport& report) {
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

Status InterpretInfo(lapack_int info, extent_t order, bool inverse,
                     LapackReport& report) {
  report.native_info = info;
  if (info < 0) {
    report.outcome = LapackOutcome::kProviderArgument;
    report.output_validity = LapackOutputValidity::kUnusable;
    if (info != std::numeric_limits<lapack_int>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  if (info > order) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = inverse ? LapackOutputValidity::kUnchanged
                                     : LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(ErrorCode::kNumerical);
  }
  return Complete(report);
}

LapackWorkspacePlan PivotPlan(LapackPlanIdentity identity, extent_t count) {
  LapackWorkspacePlan plan{identity};
  plan.regions[kInteger] = {count, count, sizeof(lapack_int),
                            alignof(lapack_int)};
  return plan;
}

Status ValidatePlan(const LapackWorkspacePlan& expected,
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
  return ValidateLapackWorkspace(supplied, expected.identity, workspace,
                                 operands);
}

template <typename T>
Result<LapackWorkspacePlan> QueryFactor(const ReferenceLapackProvider& provider,
                                        DenseBlasMatrixView<T> matrix,
                                        DenseBlasVectorView<index_t> pivots,
                                        FactorAlgorithm algorithm) {
  // The existing GETRF query performs the same descriptor/pivot/alias checks,
  // with checked formulas only, no foreign execution or numerical mutation.
  auto checked = QueryGetrfWorkspace(provider, matrix, pivots);
  if (!checked.ok()) {
    return checked.status();
  }
  const auto key = LapackPlanIdentity::Create(
      algorithm == FactorAlgorithm::kRecursive ? Native<T>::kRecursive
                                               : Native<T>::kUnblocked,
      Native<T>::kKind,
      std::array{matrix.rows(), matrix.columns(),
                 internal_lapack_layout::LeadingDimension(matrix),
                 pivots.size()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(matrix.layout()),
                                  pivots.increment(),
                                  matrix.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  auto plan = *checked;
  plan.identity = *key;
  return plan;
}

Status PublishPivots(std::span<const lapack_int> converted, extent_t rows,
                     DenseBlasVectorView<index_t> pivots,
                     LapackReport& report) {
  for (extent_t i = 0; i < pivots.size(); ++i) {
    if (converted[i] < i + 1 || converted[i] > rows) {
      report.outcome = LapackOutcome::kPartialResult;
      report.output_validity = LapackOutputValidity::kUnusable;
      return Status(ErrorCode::kProvider);
    }
  }
  for (extent_t i = 0; i < pivots.size(); ++i) {
    pivots.data()[i] = converted[i];
  }
  return Status::Ok();
}

lapack_int* ConvertPivots(RawLapackPivotView pivots,
                          const LapackWorkspace& workspace) {
  if (pivots.values().empty()) {
    return nullptr;
  }
  auto* converted = ::new (workspace.regions[kInteger].data())
      lapack_int[pivots.values().size()];
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    converted[i] = static_cast<lapack_int>(pivots.values()[i]);
  }
  return converted;
}

template <typename T>
Status Factor(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<T> matrix,
              DenseBlasVectorView<index_t> pivots, FactorAlgorithm algorithm,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  StartReport(provider,
              algorithm == FactorAlgorithm::kRecursive ? Native<T>::kRecursive
                                                       : Native<T>::kUnblocked,
              report);
  const auto expected = QueryFactor(provider, matrix, pivots, algorithm);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status = ValidatePlan(
      *expected, plan, workspace,
      std::array{matrix.reachable_storage(), pivots.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  report.factor_family = LapackFactorFamily::kLuPartialPivot;
  if (matrix.rows() == 0 || matrix.columns() == 0) {
    return Complete(report);
  }
  auto* converted = ::new (workspace.regions[kInteger].data())
      lapack_int[static_cast<std::size_t>(pivots.size())]{};
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  auto* packed = internal_lapack_layout::Pack(matrix, cursor);
  report.called_provider = true;
  const lapack_int info = Native<T>::Factor(
      algorithm, static_cast<lapack_int>(matrix.rows()),
      static_cast<lapack_int>(matrix.columns()), packed,
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(matrix)),
      converted);
  status = InterpretInfo(info, pivots.size(), false, report);
  if (info < 0 || info > pivots.size()) {
    return status;
  }
  const Status published =
      PublishPivots({converted, static_cast<std::size_t>(pivots.size())},
                    matrix.rows(), pivots, report);
  if (published.ok()) {
    internal_lapack_layout::Unpack(packed, matrix);
  }
  return published.ok() ? status : published;
}

template <typename T>
Result<LapackWorkspacePlan> QueryDriver(const ReferenceLapackProvider& provider,
                                        DenseBlasMatrixView<T> matrix,
                                        DenseBlasVectorView<index_t> pivots,
                                        DenseBlasMatrixView<T> rhs) {
  const auto checked = QueryGetrfWorkspace(provider, matrix, pivots);
  if (!checked.ok()) {
    return checked.status();
  }
  const Status status = ValidateMatrix(provider, rhs);
  if (!status.ok()) {
    return status;
  }
  if (matrix.rows() != matrix.columns() || rhs.rows() != matrix.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (Overlap(matrix.reachable_storage(), rhs.reachable_storage()) ||
      Overlap(pivots.reachable_storage(), rhs.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kDriver, Native<T>::kKind,
      std::array{matrix.rows(),
                 internal_lapack_layout::LeadingDimension(matrix),
                 rhs.columns(), internal_lapack_layout::LeadingDimension(rhs),
                 pivots.size()},
      std::array<std::int64_t, 5>{
          static_cast<std::int64_t>(matrix.layout()),
          static_cast<std::int64_t>(rhs.layout()), pivots.increment(),
          matrix.leading_dimension(), rhs.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  auto plan = *checked;
  plan.identity = *key;
  const Status packing_status = internal_lapack_layout::AddPacking(rhs, plan);
  if (!packing_status.ok()) {
    return packing_status;
  }
  return plan;
}

template <typename T>
Status Driver(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<T> matrix,
              DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<T> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  StartReport(provider, Native<T>::kDriver, report);
  const auto expected = QueryDriver(provider, matrix, pivots, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status = ValidatePlan(
      *expected, plan, workspace,
      std::array{matrix.reachable_storage(), pivots.reachable_storage(),
                 rhs.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  report.factor_family = LapackFactorFamily::kLuPartialPivot;
  if (matrix.rows() == 0) {
    return Complete(report);
  }
  auto* converted = ::new (workspace.regions[kInteger].data())
      lapack_int[static_cast<std::size_t>(pivots.size())]{};
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  auto* packed_a = internal_lapack_layout::Pack(matrix, cursor);
  auto* packed_b = internal_lapack_layout::Pack(rhs, cursor);
  // GESV factors A even with NRHS=0; only GETRS has that quick return.
  report.called_provider = true;
  const lapack_int info = Native<T>::Driver(
      static_cast<lapack_int>(matrix.rows()),
      static_cast<lapack_int>(rhs.columns()), packed_a,
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(matrix)),
      converted, packed_b,
      static_cast<lapack_int>(std::max<extent_t>(
          1, internal_lapack_layout::LeadingDimension(rhs))));
  status = InterpretInfo(info, matrix.rows(), false, report);
  if (info < 0 || info > matrix.rows()) {
    return status;
  }
  const Status published =
      PublishPivots({converted, static_cast<std::size_t>(pivots.size())},
                    matrix.rows(), pivots, report);
  if (published.ok()) {
    internal_lapack_layout::Unpack(packed_a, matrix);
    if (info == 0) {
      internal_lapack_layout::Unpack(packed_b, rhs);
    }
  }
  return published.ok() ? status : published;
}

template <typename T>
Status ValidateInverse(const ReferenceLapackProvider& provider,
                       DenseBlasMatrixView<T> factors,
                       RawLapackPivotView pivots) {
  Status status = ValidateMatrix(provider, factors);
  if (!status.ok()) {
    return status;
  }
  if (factors.rows() != factors.columns() ||
      pivots.values().size() != static_cast<std::size_t>(factors.rows())) {
    return Status(ErrorCode::kShape);
  }
  // GETRI computes N*NB before its query return. Prevent foreign overflow,
  // including in a query; do not infer a safe query just from N fitting.
  if (factors.rows() >
      std::numeric_limits<lapack_int>::max() / kGetriBlockSize) {
    return Status(ErrorCode::kOverflow);
  }
  if (Overlap(factors.reachable_storage(), pivots.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return ValidateLuPivots(pivots, factors.rows());
}

template <typename T>
Result<LapackWorkspacePlan> InversePlan(const ReferenceLapackProvider& provider,
                                        DenseBlasMatrixView<T> factors,
                                        RawLapackPivotView pivots,
                                        extent_t preferred, bool query) {
  const Status status = ValidateInverse(provider, factors, pivots);
  if (!status.ok()) {
    return status;
  }
  const extent_t minimum = std::max<extent_t>(1, factors.rows());
  if ((!query && preferred < minimum) ||
      preferred > std::numeric_limits<lapack_int>::max()) {
    return Status(ErrorCode::kInvalidState);
  }
  const auto key = LapackPlanIdentity::Create(
      query ? Native<T>::kQueryInverse : Native<T>::kInverse, Native<T>::kKind,
      std::array{factors.rows(),
                 internal_lapack_layout::LeadingDimension(factors),
                 static_cast<extent_t>(pivots.values().size())},
      std::array<std::int64_t, 5>{static_cast<std::int64_t>(factors.layout()),
                                  static_cast<std::int64_t>(pivots.family()),
                                  minimum, preferred,
                                  factors.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  auto plan = PivotPlan(*key, factors.rows());
  if (!query) {
    plan.regions[kScalar] = {minimum, preferred, sizeof(T), alignof(T)};
    const Status packing_status =
        internal_lapack_layout::AddPacking(factors, plan);
    if (!packing_status.ok()) {
      return packing_status;
    }
  }
  return plan;
}

template <typename T>
Result<LapackWorkspacePlan> QueryInverse(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<T> factors,
    RawLapackPivotView pivots, const LapackWorkspace& query_workspace,
    LapackReport& report) {
  StartReport(provider, Native<T>::kQueryInverse, report);
  const auto checked = InversePlan(provider, factors, pivots, 0, true);
  if (!checked.ok()) {
    return checked.status();
  }
  const Status status = ValidateLapackWorkspace(
      *checked, checked->identity, query_workspace,
      std::array{factors.reachable_storage(), pivots.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  const auto* converted = ConvertPivots(pivots, query_workspace);
  T query_value{};
  report.called_provider = true;
  const lapack_int info = Native<T>::Inverse(
      static_cast<lapack_int>(factors.rows()), factors.data(),
      static_cast<lapack_int>(std::max<extent_t>(
          1, internal_lapack_layout::LeadingDimension(factors))),
      converted, &query_value, -1);
  const Status interpreted = InterpretInfo(info, 0, true, report);
  report.output_validity = LapackOutputValidity::kUnchanged;
  if (!interpreted.ok()) {
    return interpreted;
  }
  if constexpr (DenseBlasComplex<T>) {
    if (query_value.imag() != 0) {
      report.outcome = LapackOutcome::kPartialResult;
      return Status(ErrorCode::kProvider);
    }
  }
  const auto preferred =
      CheckedLapackQueryEntries(static_cast<double>(std::real(query_value)),
                                provider.identity().integer_abi, sizeof(T));
  if (!preferred.ok()) {
    report.outcome = LapackOutcome::kPartialResult;
    return preferred.status();
  }
  if (*preferred < std::max<extent_t>(1, factors.rows())) {
    report.outcome = LapackOutcome::kPartialResult;
    return Status(ErrorCode::kProvider);
  }
  return InversePlan(provider, factors, pivots, *preferred, false);
}

template <typename T>
Status Inverse(const ReferenceLapackProvider& provider,
               DenseBlasMatrixView<T> factors, RawLapackPivotView pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  StartReport(provider, Native<T>::kInverse, report);
  const auto expected =
      InversePlan(provider, factors, pivots,
                  plan.regions[kScalar].preferred_entries, false);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status = ValidatePlan(
      *expected, plan, workspace,
      std::array{factors.reachable_storage(), pivots.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  if (factors.rows() == 0) {
    return Complete(report);
  }
  const auto* converted = ConvertPivots(pivots, workspace);
  const auto capacity = workspace.regions[kScalar].size() / sizeof(T);
  const auto lwork = static_cast<lapack_int>(std::min(
      capacity,
      static_cast<std::size_t>(plan.regions[kScalar].preferred_entries)));
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  auto* packed = internal_lapack_layout::Pack(factors, cursor);
  report.called_provider = true;
  status = InterpretInfo(
      Native<T>::Inverse(
          static_cast<lapack_int>(factors.rows()), packed,
          static_cast<lapack_int>(
              internal_lapack_layout::LeadingDimension(factors)),
          converted, static_cast<T*>(workspace.regions[kScalar].data()), lwork),
      factors.rows(), true, report);
  if (status.ok()) {
    internal_lapack_layout::Unpack(packed, factors);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryGetrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots, FactorAlgorithm::kRecursive);
}
Status Getrf2(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<float> matrix,
              DenseBlasVectorView<index_t> pivots,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Factor(provider, matrix, pivots, FactorAlgorithm::kRecursive, plan,
                workspace, report);
}
Result<LapackWorkspacePlan> QueryGetf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots, FactorAlgorithm::kUnblocked);
}
Status Getf2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, FactorAlgorithm::kUnblocked, plan,
                workspace, report);
}
Result<LapackWorkspacePlan> QueryGetriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> factors,
    RawLapackPivotView pivots, const LapackWorkspace& query_workspace,
    LapackReport& report) {
  return QueryInverse(provider, factors, pivots, query_workspace, report);
}
Status Getri(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> factors, RawLapackPivotView pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, factors, pivots, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGesvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<float> rhs) {
  return QueryDriver(provider, matrix, pivots, rhs);
}
Status Gesv(const ReferenceLapackProvider& provider,
            DenseBlasMatrixView<float> matrix,
            DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<float> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Driver(provider, matrix, pivots, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots, FactorAlgorithm::kRecursive);
}
Status Getrf2(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<double> matrix,
              DenseBlasVectorView<index_t> pivots,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Factor(provider, matrix, pivots, FactorAlgorithm::kRecursive, plan,
                workspace, report);
}
Result<LapackWorkspacePlan> QueryGetf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots, FactorAlgorithm::kUnblocked);
}
Status Getf2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, FactorAlgorithm::kUnblocked, plan,
                workspace, report);
}
Result<LapackWorkspacePlan> QueryGetriWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<double> factors, RawLapackPivotView pivots,
    const LapackWorkspace& query_workspace, LapackReport& report) {
  return QueryInverse(provider, factors, pivots, query_workspace, report);
}
Status Getri(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> factors, RawLapackPivotView pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, factors, pivots, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGesvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<double> rhs) {
  return QueryDriver(provider, matrix, pivots, rhs);
}
Status Gesv(const ReferenceLapackProvider& provider,
            DenseBlasMatrixView<double> matrix,
            DenseBlasVectorView<index_t> pivots,
            DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Driver(provider, matrix, pivots, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetrf2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots, FactorAlgorithm::kRecursive);
}
Status Getrf2(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<std::complex<float>> matrix,
              DenseBlasVectorView<index_t> pivots,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Factor(provider, matrix, pivots, FactorAlgorithm::kRecursive, plan,
                workspace, report);
}
Result<LapackWorkspacePlan> QueryGetf2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots, FactorAlgorithm::kUnblocked);
}
Status Getf2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, FactorAlgorithm::kUnblocked, plan,
                workspace, report);
}
Result<LapackWorkspacePlan> QueryGetriWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> factors, RawLapackPivotView pivots,
    const LapackWorkspace& query_workspace, LapackReport& report) {
  return QueryInverse(provider, factors, pivots, query_workspace, report);
}
Status Getri(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> factors,
             RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Inverse(provider, factors, pivots, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGesvWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QueryDriver(provider, matrix, pivots, rhs);
}
Status Gesv(const ReferenceLapackProvider& provider,
            DenseBlasMatrixView<std::complex<float>> matrix,
            DenseBlasVectorView<index_t> pivots,
            DenseBlasMatrixView<std::complex<float>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Driver(provider, matrix, pivots, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetrf2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots, FactorAlgorithm::kRecursive);
}
Status Getrf2(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<std::complex<double>> matrix,
              DenseBlasVectorView<index_t> pivots,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Factor(provider, matrix, pivots, FactorAlgorithm::kRecursive, plan,
                workspace, report);
}
Result<LapackWorkspacePlan> QueryGetf2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots, FactorAlgorithm::kUnblocked);
}
Status Getf2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, FactorAlgorithm::kUnblocked, plan,
                workspace, report);
}
Result<LapackWorkspacePlan> QueryGetriWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots, const LapackWorkspace& query_workspace,
    LapackReport& report) {
  return QueryInverse(provider, factors, pivots, query_workspace, report);
}
Status Getri(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> factors,
             RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Inverse(provider, factors, pivots, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGesvWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QueryDriver(provider, matrix, pivots, rhs);
}
Status Gesv(const ReferenceLapackProvider& provider,
            DenseBlasMatrixView<std::complex<double>> matrix,
            DenseBlasVectorView<index_t> pivots,
            DenseBlasMatrixView<std::complex<double>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Driver(provider, matrix, pivots, rhs, plan, workspace, report);
}

}  // namespace asc
