#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
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
#include "internal_layout.h"
#include "internal_lu_counts.h"
#include "internal_workspace_context.h"
#include "lapack_build_config.h"

// Private authoritative C/Fortran declarations. No provider types escape.
// The configuration gate and executed ABI probe select only the audited GNU
// toolchain; see programs/lapack-array-io/provider-abi-review.md.
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

constexpr std::size_t kPivotRegion =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);

consteval unsigned HexDigit(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  return 256;
}

template <std::size_t Size>
consteval bool ValidDigest(const char (&text)[Size]) {
  if (Size != 65) {
    return false;
  }
  bool nonzero = false;
  for (std::size_t i = 0; i < 64; ++i) {
    if (HexDigit(text[i]) > 15) {
      return false;
    }
    nonzero |= text[i] != '0';
  }
  return nonzero && text[64] == '\0';
}

template <std::size_t Size>
consteval std::array<std::byte, 32> Digest(const char (&text)[Size]) {
  std::array<std::byte, 32> bytes{};
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    bytes[i] = static_cast<std::byte>(16 * HexDigit(text[2 * i]) +
                                      HexDigit(text[2 * i + 1]));
  }
  return bytes;
}

static_assert(ValidDigest(ASC_LAPACK_SOURCE_SHA256));
static_assert(ValidDigest(ASC_LAPACK_BUILD_SHA256));

LapackProviderIdentity BuildIdentity() {
  LapackProviderIdentity identity;
  identity.kind = LapackProviderKind::kReference;
  identity.integer_abi = sizeof(lapack_int) == 4 ? LapackIntegerAbi::kLp64
                                                 : LapackIntegerAbi::kIlp64;
  identity.logical_bytes = sizeof(lapack_int);
  identity.source_sha256 = Digest(ASC_LAPACK_SOURCE_SHA256);
  identity.build_sha256 = Digest(ASC_LAPACK_BUILD_SHA256);
  identity.version = {3, 12, 1};
  return identity;
}

// An untouched or partially written INFO must not manufacture success.
// Full-width MIN retains an impossible sign bit after a short zero write on
// the audited little-endian provider ABI.
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr LapackScalarKind kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kGetrf = "sgetrf";
  static constexpr std::string_view kGetrs = "sgetrs";
  static lapack_int Factor(lapack_int m, lapack_int n, float* a, lapack_int lda,
                           lapack_int* pivots) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgetrf(&m, &n, a, &lda, pivots, &info);
    return info;
  }
  static lapack_int Solve(char trans, lapack_int n, lapack_int nrhs,
                          const float* a, lapack_int lda,
                          const lapack_int* pivots, float* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgetrs(&trans, &n, &nrhs, a, &lda, pivots, b, &ldb, &info);
    return info;
  }
};
template <>
struct Native<double> {
  static constexpr LapackScalarKind kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kGetrf = "dgetrf";
  static constexpr std::string_view kGetrs = "dgetrs";
  static lapack_int Factor(lapack_int m, lapack_int n, double* a,
                           lapack_int lda, lapack_int* pivots) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgetrf(&m, &n, a, &lda, pivots, &info);
    return info;
  }
  static lapack_int Solve(char trans, lapack_int n, lapack_int nrhs,
                          const double* a, lapack_int lda,
                          const lapack_int* pivots, double* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgetrs(&trans, &n, &nrhs, a, &lda, pivots, b, &ldb, &info);
    return info;
  }
};
template <>
struct Native<std::complex<float>> {
  static constexpr LapackScalarKind kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kGetrf = "cgetrf";
  static constexpr std::string_view kGetrs = "cgetrs";
  static lapack_int Factor(lapack_int m, lapack_int n, std::complex<float>* a,
                           lapack_int lda, lapack_int* pivots) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgetrf(&m, &n, a, &lda, pivots, &info);
    return info;
  }
  static lapack_int Solve(char trans, lapack_int n, lapack_int nrhs,
                          const std::complex<float>* a, lapack_int lda,
                          const lapack_int* pivots, std::complex<float>* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgetrs(&trans, &n, &nrhs, a, &lda, pivots, b, &ldb, &info);
    return info;
  }
};
template <>
struct Native<std::complex<double>> {
  static constexpr LapackScalarKind kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kGetrf = "zgetrf";
  static constexpr std::string_view kGetrs = "zgetrs";
  static lapack_int Factor(lapack_int m, lapack_int n, std::complex<double>* a,
                           lapack_int lda, lapack_int* pivots) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgetrf(&m, &n, a, &lda, pivots, &info);
    return info;
  }
  static lapack_int Solve(char trans, lapack_int n, lapack_int nrhs,
                          const std::complex<double>* a, lapack_int lda,
                          const lapack_int* pivots, std::complex<double>* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgetrs(&trans, &n, &nrhs, a, &lda, pivots, b, &ldb, &info);
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
  if (matrix.memory_space() != MemorySpace::kHost &&
      matrix.memory_space() != MemorySpace::kPinnedHost) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (!provider.context().CanAccess(matrix.memory_space())) {
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

LapackWorkspacePlan PivotPlan(LapackPlanIdentity identity, extent_t count) {
  LapackWorkspacePlan plan{identity};
  plan.regions[kPivotRegion] = {count, count, sizeof(lapack_int),
                                alignof(lapack_int)};
  return plan;
}

template <typename T>
Result<LapackWorkspacePlan> QueryFactor(const ReferenceLapackProvider& provider,
                                        DenseBlasMatrixView<T> matrix,
                                        DenseBlasVectorView<index_t> pivots) {
  Status status = ValidateMatrix(provider, matrix);
  if (!status.ok()) {
    return status;
  }
  status = internal_lapack_lu::CheckFactor(
      internal_lapack_lu::FactorRoute::kBlocked, matrix.rows(),
      matrix.columns(), internal_lapack_layout::LeadingDimension(matrix),
      std::numeric_limits<lapack_int>::max());
  if (!status.ok()) {
    return status;
  }
  if (pivots.size() != std::min(matrix.rows(), matrix.columns())) {
    return Status(ErrorCode::kShape);
  }
  if ((pivots.memory_space() != MemorySpace::kHost &&
       pivots.memory_space() != MemorySpace::kPinnedHost) ||
      !provider.context().CanAccess(pivots.memory_space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (pivots.increment() != 1 ||
      Overlap(matrix.reachable_storage(), pivots.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kGetrf, Native<T>::kScalar,
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
  auto plan = PivotPlan(*key, pivots.size());
  status = internal_lapack_layout::AddPacking(matrix, plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(const ReferenceLapackProvider& provider,
                                       DenseBlasTranspose transpose,
                                       LapackLuFactorView<T> factor,
                                       DenseBlasMatrixView<T> rhs) {
  const auto matrix = factor.factors();
  Status status = ValidateMatrix(provider, matrix);
  if (!status.ok()) {
    return status;
  }
  status = ValidateMatrix(provider, rhs);
  if (!status.ok()) {
    return status;
  }
  if (transpose != DenseBlasTranspose::kNone &&
      transpose != DenseBlasTranspose::kTranspose &&
      transpose != DenseBlasTranspose::kConjugateTranspose) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  if (matrix.rows() != matrix.columns() || rhs.rows() != matrix.rows()) {
    return Status(ErrorCode::kShape);
  }
  status = internal_lapack_lu::CheckSolve(
      matrix.rows(), rhs.columns(), std::numeric_limits<lapack_int>::max());
  if (!status.ok()) {
    return status;
  }
  if (!provider.context().CanAccess(
          factor.pivots().reachable_storage().space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  status = ValidateLuPivots(factor.pivots(), matrix.rows());
  if (!status.ok()) {
    return status;
  }
  if (Overlap(matrix.reachable_storage(), rhs.reachable_storage()) ||
      Overlap(factor.pivots().reachable_storage(), rhs.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kGetrs, Native<T>::kScalar,
      std::array{matrix.rows(), matrix.columns(),
                 internal_lapack_layout::LeadingDimension(matrix), rhs.rows(),
                 rhs.columns(), internal_lapack_layout::LeadingDimension(rhs),
                 static_cast<extent_t>(factor.pivots().values().size())},
      std::array<std::int64_t, 5>{static_cast<std::int64_t>(transpose),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  matrix.leading_dimension(),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  auto plan = PivotPlan(*key, matrix.rows());
  status = internal_lapack_layout::AddPacking(matrix, plan);
  if (!status.ok()) {
    return status;
  }
  status = internal_lapack_layout::AddPacking(rhs, plan);
  if (!status.ok()) {
    return status;
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
void StartReport(const ReferenceLapackProvider& provider, bool solve,
                 LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  const std::string_view name = solve ? Native<T>::kGetrs : Native<T>::kGetrf;
  std::copy(name.begin(), name.end(), report.routine.begin());
}

Status InterpretInfo(lapack_int info, bool solve, LapackReport& report) {
  report.native_info = info;
  if (info < 0) {
    report.outcome = LapackOutcome::kProviderArgument;
    report.output_validity = LapackOutputValidity::kUnusable;
    // Avoid signed negation overflow even for malformed provider output.
    if (info != std::numeric_limits<lapack_int>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  if (info > 0) {
    report.outcome =
        solve ? LapackOutcome::kPartialResult : LapackOutcome::kSingular;
    report.output_validity = solve ? LapackOutputValidity::kUnusable
                                   : LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(solve ? ErrorCode::kProvider : ErrorCode::kNumerical);
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

template <typename T>
Status Factor(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<T> matrix,
              DenseBlasVectorView<index_t> pivots,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  StartReport<T>(provider, false, report);
  const auto expected = QueryFactor(provider, matrix, pivots);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status = ValidatePlan(
      provider, *expected, plan, workspace,
      std::array{matrix.reachable_storage(), pivots.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  report.factor_family = LapackFactorFamily::kLuPartialPivot;
  if (matrix.rows() == 0 || matrix.columns() == 0) {
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  // The standard nonallocating placement array form has zero array overhead
  // (CWG 2382). This starts a real ABI-integer array lifetime in caller bytes.
  auto* converted = ::new (workspace.regions[kPivotRegion].data())
      lapack_int[static_cast<std::size_t>(pivots.size())]{};
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  auto* packed = internal_lapack_layout::Pack(matrix, cursor);
  report.called_provider = true;
  const lapack_int info = Native<T>::Factor(
      static_cast<lapack_int>(matrix.rows()),
      static_cast<lapack_int>(matrix.columns()), packed,
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(matrix)),
      converted);
  status = InterpretInfo(info, false, report);
  if (info < 0) {
    return status;
  }
  if (info > pivots.size()) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  for (extent_t i = 0; i < pivots.size(); ++i) {
    if (converted[i] < i + 1 || converted[i] > matrix.rows()) {
      report.outcome = LapackOutcome::kPartialResult;
      report.output_validity = LapackOutputValidity::kUnusable;
      return Status(ErrorCode::kProvider);
    }
  }
  for (extent_t i = 0; i < pivots.size(); ++i) {
    pivots.data()[i] = converted[i];
  }
  internal_lapack_layout::Unpack(packed, matrix);
  return status;
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose, LapackLuFactorView<T> factor,
             DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  StartReport<T>(provider, true, report);
  const auto expected = QuerySolve(provider, transpose, factor, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  const auto matrix = factor.factors();
  Status status = ValidatePlan(
      provider, *expected, plan, workspace,
      std::array{matrix.reachable_storage(),
                 factor.pivots().reachable_storage(), rhs.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    if (matrix.data()[i * matrix.leading_dimension() + i] == T{}) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  if (matrix.rows() == 0 || rhs.columns() == 0) {
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  auto* converted = ::new (workspace.regions[kPivotRegion].data())
      lapack_int[static_cast<std::size_t>(matrix.rows())];
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    converted[i] = static_cast<lapack_int>(factor.pivots().values()[i]);
  }
  char trans = 'N';
  if (transpose == DenseBlasTranspose::kTranspose) {
    trans = 'T';
  }
  if (transpose == DenseBlasTranspose::kConjugateTranspose) {
    trans = 'C';
  }
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  const auto* packed_a = internal_lapack_layout::Pack(matrix, cursor);
  auto* packed_b = internal_lapack_layout::Pack(rhs, cursor);
  report.called_provider = true;
  status = InterpretInfo(
      Native<T>::Solve(trans, static_cast<lapack_int>(matrix.rows()),
                       static_cast<lapack_int>(rhs.columns()), packed_a,
                       static_cast<lapack_int>(
                           internal_lapack_layout::LeadingDimension(matrix)),
                       converted, packed_b,
                       static_cast<lapack_int>(
                           internal_lapack_layout::LeadingDimension(rhs))),
      true, report);
  if (status.ok()) {
    internal_lapack_layout::Unpack(packed_b, rhs);
  }
  return status;
}

}  // namespace

Result<ReferenceLapackProvider> ReferenceLapackProvider::Create(
    ExecutionContext context) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported);
  }
  return ReferenceLapackProvider(std::move(context), BuildIdentity());
}

Result<LapackWorkspacePlan> QueryGetrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots);
}
Result<LapackWorkspacePlan> QueryGetrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackLuFactorView<float> factor, DenseBlasMatrixView<float> rhs) {
  return QuerySolve(provider, transpose, factor, rhs);
}
Status Getrf(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, plan, workspace, report);
}
Status Getrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose, LapackLuFactorView<float> factor,
             DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, transpose, factor, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots);
}
Result<LapackWorkspacePlan> QueryGetrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackLuFactorView<double> factor, DenseBlasMatrixView<double> rhs) {
  return QuerySolve(provider, transpose, factor, rhs);
}
Status Getrf(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, plan, workspace, report);
}
Status Getrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose, LapackLuFactorView<double> factor,
             DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, transpose, factor, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetrfWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots);
}
Result<LapackWorkspacePlan> QueryGetrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackLuFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QuerySolve(provider, transpose, factor, rhs);
}
Status Getrf(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, plan, workspace, report);
}
Status Getrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackLuFactorView<std::complex<float>> factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, transpose, factor, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetrfWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, matrix, pivots);
}
Result<LapackWorkspacePlan> QueryGetrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackLuFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QuerySolve(provider, transpose, factor, rhs);
}
Status Getrf(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, pivots, plan, workspace, report);
}
Status Getrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackLuFactorView<std::complex<double>> factor,
             DenseBlasMatrixView<std::complex<double>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, transpose, factor, rhs, plan, workspace, report);
}

}  // namespace asc
