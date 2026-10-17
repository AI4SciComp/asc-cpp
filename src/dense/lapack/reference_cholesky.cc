#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
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
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky.h"
#include "internal_cholesky_limits.h"
#include "internal_layout.h"
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
static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);

enum class Operation : std::uint8_t {
  kBlocked,
  kRecursive,
  kUnblocked,
  kInverse,
  kSolve,
  kDriver
};

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF32;
  static constexpr std::array<std::string_view, 6> kNames{
      "spotrf", "spotrf2", "spotf2", "spotri", "spotrs", "sposv"};
  static lapack_int Factor(Operation operation, char triangle, lapack_int n,
                           float* a, lapack_int lda) {
    lapack_int info = 0;
    switch (operation) {
      case Operation::kBlocked:
        LAPACK_spotrf(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kRecursive:
        LAPACK_spotrf2(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kUnblocked:
        LAPACK_spotf2(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kInverse:
        LAPACK_spotri(&triangle, &n, a, &lda, &info);
        break;
      default:
        break;
    }
    return info;
  }
  static lapack_int Solve(char triangle, lapack_int n, lapack_int nrhs,
                          const float* a, lapack_int lda, float* b,
                          lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_spotrs(&triangle, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
  static lapack_int Driver(char triangle, lapack_int n, lapack_int nrhs,
                           float* a, lapack_int lda, float* b, lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_sposv(&triangle, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF64;
  static constexpr std::array<std::string_view, 6> kNames{
      "dpotrf", "dpotrf2", "dpotf2", "dpotri", "dpotrs", "dposv"};
  static lapack_int Factor(Operation operation, char triangle, lapack_int n,
                           double* a, lapack_int lda) {
    lapack_int info = 0;
    switch (operation) {
      case Operation::kBlocked:
        LAPACK_dpotrf(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kRecursive:
        LAPACK_dpotrf2(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kUnblocked:
        LAPACK_dpotf2(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kInverse:
        LAPACK_dpotri(&triangle, &n, a, &lda, &info);
        break;
      default:
        break;
    }
    return info;
  }
  static lapack_int Solve(char triangle, lapack_int n, lapack_int nrhs,
                          const double* a, lapack_int lda, double* b,
                          lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_dpotrs(&triangle, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
  static lapack_int Driver(char triangle, lapack_int n, lapack_int nrhs,
                           double* a, lapack_int lda, double* b,
                           lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_dposv(&triangle, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC64;
  static constexpr std::array<std::string_view, 6> kNames{
      "cpotrf", "cpotrf2", "cpotf2", "cpotri", "cpotrs", "cposv"};
  static lapack_int Factor(Operation operation, char triangle, lapack_int n,
                           std::complex<float>* a, lapack_int lda) {
    lapack_int info = 0;
    switch (operation) {
      case Operation::kBlocked:
        LAPACK_cpotrf(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kRecursive:
        LAPACK_cpotrf2(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kUnblocked:
        LAPACK_cpotf2(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kInverse:
        LAPACK_cpotri(&triangle, &n, a, &lda, &info);
        break;
      default:
        break;
    }
    return info;
  }
  static lapack_int Solve(char triangle, lapack_int n, lapack_int nrhs,
                          const std::complex<float>* a, lapack_int lda,
                          std::complex<float>* b, lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_cpotrs(&triangle, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
  static lapack_int Driver(char triangle, lapack_int n, lapack_int nrhs,
                           std::complex<float>* a, lapack_int lda,
                           std::complex<float>* b, lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_cposv(&triangle, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC128;
  static constexpr std::array<std::string_view, 6> kNames{
      "zpotrf", "zpotrf2", "zpotf2", "zpotri", "zpotrs", "zposv"};
  static lapack_int Factor(Operation operation, char triangle, lapack_int n,
                           std::complex<double>* a, lapack_int lda) {
    lapack_int info = 0;
    switch (operation) {
      case Operation::kBlocked:
        LAPACK_zpotrf(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kRecursive:
        LAPACK_zpotrf2(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kUnblocked:
        LAPACK_zpotf2(&triangle, &n, a, &lda, &info);
        break;
      case Operation::kInverse:
        LAPACK_zpotri(&triangle, &n, a, &lda, &info);
        break;
      default:
        break;
    }
    return info;
  }
  static lapack_int Solve(char triangle, lapack_int n, lapack_int nrhs,
                          const std::complex<double>* a, lapack_int lda,
                          std::complex<double>* b, lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_zpotrs(&triangle, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
  static lapack_int Driver(char triangle, lapack_int n, lapack_int nrhs,
                           std::complex<double>* a, lapack_int lda,
                           std::complex<double>* b, lapack_int ldb) {
    lapack_int info = 0;
    LAPACK_zposv(&triangle, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
};

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  const auto a = reinterpret_cast<std::uintptr_t>(first.data());
  const auto b = reinterpret_cast<std::uintptr_t>(second.data());
  return first.size() != 0 && second.size() != 0 &&
         (a <= b ? b - a < first.size() : a - b < second.size());
}

template <typename T>
ConstMemoryView ObjectStorage(const T& object) {
  return {&object, sizeof(object), MemorySpace::kHost};
}

template <typename T>
Status ValidateMatrix(const ReferenceLapackProvider& provider,
                      DenseBlasMatrixView<T> matrix) {
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

template <typename T>
Result<LapackWorkspacePlan> QuerySingle(const ReferenceLapackProvider& provider,
                                        DenseBlasTriangle triangle,
                                        DenseBlasMatrixView<T> matrix,
                                        Operation operation) {
  if (!internal_dense_lapack::IsTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const Status valid = ValidateMatrix(provider, matrix);
  if (!valid.ok()) {
    return valid;
  }
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape);
  }
  // Pinned ILAENV(ISPEC=1) returns 64 for PO/TRF, TR/TRI and LA/UUM.
  // POTRF/POSV and POTRI's TRTRI/LAUUM use those blocked-loop increments.
  const bool blocked = operation == Operation::kBlocked ||
                       operation == Operation::kDriver ||
                       operation == Operation::kInverse;
  const Status arithmetic = internal_cholesky_limits::CheckOrder(
      matrix.rows(), internal_lapack_layout::LeadingDimension(matrix),
      std::numeric_limits<lapack_int>::max(), blocked ? 64 : 1,
      operation == Operation::kUnblocked || operation == Operation::kInverse);
  if (!arithmetic.ok()) {
    return arithmetic;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<std::remove_const_t<T>>::kNames[static_cast<std::size_t>(
          operation)],
      Native<std::remove_const_t<T>>::kKind,
      std::array{matrix.rows(), matrix.columns(),
                 internal_lapack_layout::LeadingDimension(matrix)},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  matrix.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const Status packed = internal_lapack_layout::AddPacking(matrix, plan);
  if (!packed.ok()) {
    return packed;
  }
  return plan;
}

template <typename T, typename U>
Result<LapackWorkspacePlan> QueryPair(const ReferenceLapackProvider& provider,
                                      DenseBlasTriangle triangle,
                                      DenseBlasMatrixView<T> matrix,
                                      DenseBlasMatrixView<U> rhs,
                                      Operation operation) {
  auto checked = QuerySingle(provider, triangle, matrix, operation);
  if (!checked.ok()) {
    return checked.status();
  }
  const Status valid = ValidateMatrix(provider, rhs);
  if (!valid.ok()) {
    return valid;
  }
  if (rhs.rows() != matrix.rows()) {
    return Status(ErrorCode::kShape);
  }
  const Status arithmetic = internal_cholesky_limits::CheckRightHandSides(
      matrix.rows(), rhs.columns(), std::numeric_limits<lapack_int>::max());
  if (!arithmetic.ok()) {
    return arithmetic;
  }
  if (Overlap(matrix.reachable_storage(), rhs.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto key = LapackPlanIdentity::Create(
      Native<U>::kNames[static_cast<std::size_t>(operation)], Native<U>::kKind,
      std::array{matrix.rows(), matrix.columns(),
                 internal_lapack_layout::LeadingDimension(matrix), rhs.rows(),
                 rhs.columns(), internal_lapack_layout::LeadingDimension(rhs)},
      std::array<std::int64_t, 5>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  matrix.leading_dimension(),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  checked->identity = *key;
  const Status packed = internal_lapack_layout::AddPacking(rhs, *checked);
  if (!packed.ok()) {
    return packed;
  }
  return *checked;
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(const ReferenceLapackProvider& provider,
                                       LapackCholeskyFactorView<T> factor,
                                       DenseBlasMatrixView<T> rhs) {
  if (factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  return QueryPair(provider, factor.triangle(), factor.factors(), rhs,
                   Operation::kSolve);
}

// Check live metadata before resetting the report; alias errors cannot safely
// reset it. Each operand/region descriptor already carries a checked byte span.
Status CheckMetadata(const ReferenceLapackProvider& provider,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report,
                     std::span<const ConstMemoryView> operands) {
  const std::array metadata{ObjectStorage(provider), ObjectStorage(plan),
                            ObjectStorage(workspace), ObjectStorage(report)};
  for (std::size_t i = 0; i < metadata.size(); ++i) {
    for (std::size_t j = i + 1; j < metadata.size(); ++j) {
      if (Overlap(metadata[i], metadata[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (const auto operand : operands) {
      if (Overlap(metadata[i], operand)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (const auto& region : workspace.regions) {
      if (Overlap(metadata[i], region)) {
        return Status(ErrorCode::kInvalidArgument);
      }
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
T& Entry(DenseBlasMatrixView<T> matrix, extent_t row, extent_t column) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? matrix.data()[column * matrix.leading_dimension() + row]
             : matrix.data()[row * matrix.leading_dimension() + column];
}

template <typename T>
T HermitianDiagonal(const T& value) {
  if constexpr (std::is_floating_point_v<T>) {
    return value;
  } else {
    return T(value.real(), 0);
  }
}

template <typename T>
T* PackTriangle(DenseBlasMatrixView<T> matrix, DenseBlasTriangle triangle,
                bool hermitian_input, std::remove_const_t<T>*& cursor) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor || matrix.rows() == 0) {
    return matrix.data();
  }
  auto* packed = cursor;
  const extent_t n = matrix.rows();
  for (extent_t column = 0; column < n; ++column) {
    const extent_t begin = triangle == DenseBlasTriangle::kUpper ? 0 : column;
    const extent_t end = triangle == DenseBlasTriangle::kUpper ? column + 1 : n;
    for (extent_t row = begin; row < end; ++row) {
      const auto& value = Entry(matrix, row, column);
      packed[column * n + row] =
          hermitian_input && row == column ? HermitianDiagonal(value) : value;
    }
  }
  cursor += n * n;
  return packed;
}

template <typename T>
void PublishTriangle(const T* packed, DenseBlasMatrixView<T> matrix,
                     DenseBlasTriangle triangle, bool partial_hermitian) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return;
  }
  const extent_t n = matrix.rows();
  for (extent_t column = 0; column < n; ++column) {
    const extent_t begin = triangle == DenseBlasTriangle::kUpper ? 0 : column;
    const extent_t end = triangle == DenseBlasTriangle::kUpper ? column + 1 : n;
    for (extent_t row = begin; row < end; ++row) {
      auto& destination = Entry(matrix, row, column);
      const auto& value = packed[column * n + row];
      if constexpr (!std::is_floating_point_v<T>) {
        if (partial_hermitian && row == column) {
          destination.real(value.real());
          continue;
        }
      }
      destination = value;
    }
  }
}

char Triangle(DenseBlasTriangle triangle) {
  return triangle == DenseBlasTriangle::kUpper ? 'U' : 'L';
}

Status Complete(LapackReport& report) {
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

Status InterpretInfo(lapack_int info, extent_t n, Operation operation,
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
  if (info > n || (operation == Operation::kSolve && info != 0)) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  if (info > 0) {
    const bool inverse = operation == Operation::kInverse;
    report.outcome = inverse ? LapackOutcome::kSingular
                             : LapackOutcome::kNotPositiveDefinite;
    report.output_validity = inverse ? LapackOutputValidity::kUnchanged
                                     : LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(ErrorCode::kNumerical);
  }
  return Complete(report);
}

template <typename T>
Status ExecuteSingle(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
                     Operation operation, const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{matrix.reachable_storage()};
  Status status = CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  StartReport(provider, Native<T>::kNames[static_cast<std::size_t>(operation)],
              report);
  const auto expected = QuerySingle(provider, triangle, matrix, operation);
  if (!expected.ok()) {
    return expected.status();
  }
  status = ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const bool inverse = operation == Operation::kInverse;
  if (!inverse) {
    report.factor_family = LapackFactorFamily::kCholesky;
  }
  if (matrix.rows() == 0) {
    return Complete(report);
  }
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  auto* packed = PackTriangle(matrix, triangle, !inverse, cursor);
  report.called_provider = true;
  const lapack_int info =
      Native<T>::Factor(operation, Triangle(triangle),
                        static_cast<lapack_int>(matrix.rows()), packed,
                        static_cast<lapack_int>(
                            internal_lapack_layout::LeadingDimension(matrix)));
  status = InterpretInfo(info, matrix.rows(), operation, report);
  if (info >= 0 && info <= matrix.rows() && (!inverse || info == 0)) {
    PublishTriangle(packed, matrix, triangle, info > 0);
  }
  return status;
}

template <typename T>
Status ExecuteSolve(const ReferenceLapackProvider& provider,
                    LapackCholeskyFactorView<T> factor,
                    DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace, LapackReport& report) {
  const auto matrix = factor.factors();
  const std::array operands{matrix.reachable_storage(),
                            rhs.reachable_storage()};
  Status status = CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  StartReport(provider, Native<T>::kNames[4], report);
  const auto expected = QuerySolve(provider, factor, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  status = ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (matrix.rows() == 0 || rhs.columns() == 0) {
    return Complete(report);
  }
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  const auto* packed = PackTriangle(matrix, factor.triangle(), false, cursor);
  auto* packed_rhs = internal_lapack_layout::Pack(rhs, cursor);
  report.called_provider = true;
  const lapack_int info = Native<T>::Solve(
      Triangle(factor.triangle()), static_cast<lapack_int>(matrix.rows()),
      static_cast<lapack_int>(rhs.columns()), packed,
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(matrix)),
      packed_rhs,
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(rhs)));
  status = InterpretInfo(info, matrix.rows(), Operation::kSolve, report);
  if (info == 0) {
    internal_lapack_layout::Unpack(packed_rhs, rhs);
  }
  return status;
}

template <typename T>
Status ExecuteDriver(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
                     DenseBlasMatrixView<T> rhs,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{matrix.reachable_storage(),
                            rhs.reachable_storage()};
  Status status = CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  StartReport(provider, Native<T>::kNames[5], report);
  const auto expected =
      QueryPair(provider, triangle, matrix, rhs, Operation::kDriver);
  if (!expected.ok()) {
    return expected.status();
  }
  status = ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  report.factor_family = LapackFactorFamily::kCholesky;
  if (matrix.rows() == 0) {
    return Complete(report);
  }
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  auto* packed = PackTriangle(matrix, triangle, true, cursor);
  auto* packed_rhs = internal_lapack_layout::Pack(rhs, cursor);
  // A dummy live B object avoids passing null across the foreign boundary
  // for n>0,nrhs=0; POSV still factors A before POTRS's empty-RHS quick return.
  T dummy_rhs{};
  if (rhs.columns() == 0) {
    packed_rhs = &dummy_rhs;
  }
  report.called_provider = true;
  const lapack_int info = Native<T>::Driver(
      Triangle(triangle), static_cast<lapack_int>(matrix.rows()),
      static_cast<lapack_int>(rhs.columns()), packed,
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(matrix)),
      packed_rhs,
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(rhs)));
  status = InterpretInfo(info, matrix.rows(), Operation::kDriver, report);
  if (info >= 0 && info <= matrix.rows()) {
    PublishTriangle(packed, matrix, triangle, info > 0);
    if (info == 0) {
      internal_lapack_layout::Unpack(packed_rhs, rhs);
    }
  }
  return status;
}

}  // namespace

Result<LapackWorkspacePlan> QueryPotrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kBlocked);
}
Status Potrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kBlocked, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kRecursive);
}
Status Potrf2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kRecursive, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kUnblocked);
}
Status Potf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kUnblocked, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kInverse);
}
Status Potri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kInverse, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackCholeskyFactorView<float> factor, DenseBlasMatrixView<float> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Status Potrs(const ReferenceLapackProvider& provider,
             LapackCholeskyFactorView<float> factor,
             DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPosvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs) {
  return QueryPair(provider, triangle, matrix, rhs, Operation::kDriver);
}
Status Posv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return ExecuteDriver(provider, triangle, matrix, rhs, plan, workspace,
                       report);
}

Result<LapackWorkspacePlan> QueryPotrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kBlocked);
}
Status Potrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kBlocked, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kRecursive);
}
Status Potrf2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kRecursive, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kUnblocked);
}
Status Potf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kUnblocked, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kInverse);
}
Status Potri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kInverse, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackCholeskyFactorView<double> factor, DenseBlasMatrixView<double> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Status Potrs(const ReferenceLapackProvider& provider,
             LapackCholeskyFactorView<double> factor,
             DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPosvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs) {
  return QueryPair(provider, triangle, matrix, rhs, Operation::kDriver);
}
Status Posv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return ExecuteDriver(provider, triangle, matrix, rhs, plan, workspace,
                       report);
}

Result<LapackWorkspacePlan> QueryPotrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kBlocked);
}
Status Potrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<float>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kBlocked, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kRecursive);
}
Status Potrf2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<float>> matrix,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kRecursive, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kUnblocked);
}
Status Potf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<float>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kUnblocked, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kInverse);
}
Status Potri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<float>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kInverse, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackCholeskyFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Status Potrs(const ReferenceLapackProvider& provider,
             LapackCholeskyFactorView<std::complex<float>> factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPosvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QueryPair(provider, triangle, matrix, rhs, Operation::kDriver);
}
Status Posv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasMatrixView<std::complex<float>> matrix,
            DenseBlasMatrixView<std::complex<float>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return ExecuteDriver(provider, triangle, matrix, rhs, plan, workspace,
                       report);
}

Result<LapackWorkspacePlan> QueryPotrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kBlocked);
}
Status Potrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<double>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kBlocked, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kRecursive);
}
Status Potrf2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<double>> matrix,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kRecursive, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kUnblocked);
}
Status Potf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<double>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kUnblocked, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix) {
  return QuerySingle(provider, triangle, matrix, Operation::kInverse);
}
Status Potri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<double>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSingle(provider, triangle, matrix, Operation::kInverse, plan,
                       workspace, report);
}

Result<LapackWorkspacePlan> QueryPotrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackCholeskyFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Status Potrs(const ReferenceLapackProvider& provider,
             LapackCholeskyFactorView<std::complex<double>> factor,
             DenseBlasMatrixView<std::complex<double>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryPosvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QueryPair(provider, triangle, matrix, rhs, Operation::kDriver);
}
Status Posv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasMatrixView<std::complex<double>> matrix,
            DenseBlasMatrixView<std::complex<double>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return ExecuteDriver(provider, triangle, matrix, rhs, plan, workspace,
                       report);
}

}  // namespace asc
