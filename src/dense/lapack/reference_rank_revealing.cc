#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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
#include "asc/dense/providers/lapack_rank_revealing.h"
#include "internal_layout.h"
#include "internal_rank_revealing_counts.h"
#include "lapack_build_config.h"

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#elif ASC_LAPACK_INTEGER_BITS != 32
#error "Reference LAPACK requires the explicitly selected 32/64 integer ABI"
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
using internal_lapack_rank_revealing::Operation;
constexpr extent_t kIntegerLimit = std::numeric_limits<lapack_int>::max();
constexpr std::size_t kScalar =
    static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr std::size_t kReal =
    static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr std::size_t kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr std::size_t kPermutation =
    static_cast<std::size_t>(LapackWorkspaceKind::kPivotConversion);
constexpr std::size_t kPacking =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF32;
  static constexpr std::array<std::string_view, 2> kNames{"sgeqp3", "sgelsy"};
  static lapack_int Invoke(Operation operation, lapack_int m, lapack_int n,
                           lapack_int nrhs, float* a, lapack_int lda, float* b,
                           lapack_int ldb, lapack_int* pivots, float* tau,
                           float rcond, lapack_int& rank, float* work,
                           lapack_int lwork, float* /*rwork*/) {
    lapack_int info = 0;
    if (operation == Operation::kGeqp3) {
      LAPACK_sgeqp3(&m, &n, a, &lda, pivots, tau, work, &lwork, &info);
    } else {
      LAPACK_sgelsy(&m, &n, &nrhs, a, &lda, b, &ldb, pivots, &rcond, &rank,
                    work, &lwork, &info);
    }
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF64;
  static constexpr std::array<std::string_view, 2> kNames{"dgeqp3", "dgelsy"};
  static lapack_int Invoke(Operation operation, lapack_int m, lapack_int n,
                           lapack_int nrhs, double* a, lapack_int lda,
                           double* b, lapack_int ldb, lapack_int* pivots,
                           double* tau, double rcond, lapack_int& rank,
                           double* work, lapack_int lwork, double* /*rwork*/) {
    lapack_int info = 0;
    if (operation == Operation::kGeqp3) {
      LAPACK_dgeqp3(&m, &n, a, &lda, pivots, tau, work, &lwork, &info);
    } else {
      LAPACK_dgelsy(&m, &n, &nrhs, a, &lda, b, &ldb, pivots, &rcond, &rank,
                    work, &lwork, &info);
    }
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC64;
  static constexpr std::array<std::string_view, 2> kNames{"cgeqp3", "cgelsy"};
  static lapack_int Invoke(Operation operation, lapack_int m, lapack_int n,
                           lapack_int nrhs, std::complex<float>* a,
                           lapack_int lda, std::complex<float>* b,
                           lapack_int ldb, lapack_int* pivots,
                           std::complex<float>* tau, float rcond,
                           lapack_int& rank, std::complex<float>* work,
                           lapack_int lwork, float* rwork) {
    lapack_int info = 0;
    if (operation == Operation::kGeqp3) {
      LAPACK_cgeqp3(&m, &n, a, &lda, pivots, tau, work, &lwork, rwork, &info);
    } else {
      LAPACK_cgelsy(&m, &n, &nrhs, a, &lda, b, &ldb, pivots, &rcond, &rank,
                    work, &lwork, rwork, &info);
    }
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC128;
  static constexpr std::array<std::string_view, 2> kNames{"zgeqp3", "zgelsy"};
  static lapack_int Invoke(Operation operation, lapack_int m, lapack_int n,
                           lapack_int nrhs, std::complex<double>* a,
                           lapack_int lda, std::complex<double>* b,
                           lapack_int ldb, lapack_int* pivots,
                           std::complex<double>* tau, double rcond,
                           lapack_int& rank, std::complex<double>* work,
                           lapack_int lwork, double* rwork) {
    lapack_int info = 0;
    if (operation == Operation::kGeqp3) {
      LAPACK_zgeqp3(&m, &n, a, &lda, pivots, tau, work, &lwork, rwork, &info);
    } else {
      LAPACK_zgelsy(&m, &n, &nrhs, a, &lda, b, &ldb, pivots, &rcond, &rank,
                    work, &lwork, rwork, &info);
    }
    return info;
  }
};

template <typename T>
struct Call {
  Operation operation;
  DenseBlasMatrixView<T> matrix;
  DenseBlasVectorView<index_t> pivots;
  std::optional<DenseBlasMatrixView<T>> rhs;
  std::optional<DenseBlasVectorView<T>> tau;
  DenseBlasRealType<T> rcond = 0;
  index_t* rank = nullptr;
};

template <typename T>
std::array<ConstMemoryView, 5> Operands(const Call<T>& call) {
  const ConstMemoryView empty(nullptr, 0, MemorySpace::kHost);
  return {call.matrix.reachable_storage(), call.pivots.reachable_storage(),
          call.rhs ? call.rhs->reachable_storage() : empty,
          call.tau ? call.tau->reachable_storage() : empty,
          call.rank
              ? ConstMemoryView(call.rank, sizeof(index_t), MemorySpace::kHost)
              : empty};
}

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

Status CheckDisjoint(std::span<const ConstMemoryView> objects) {
  for (std::size_t i = 0; i < objects.size(); ++i) {
    for (std::size_t j = i + 1; j < objects.size(); ++j) {
      if (Overlap(objects[i], objects[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

Status CheckMetadata(std::span<const ConstMemoryView> metadata,
                     std::span<const ConstMemoryView> operands,
                     const LapackWorkspace* workspace) {
  Status disjoint = CheckDisjoint(metadata);
  if (!disjoint.ok()) {
    return disjoint;
  }
  for (const auto object : metadata) {
    for (const auto operand : operands) {
      if (Overlap(object, operand)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    if (workspace != nullptr) {
      for (const auto region : workspace->regions) {
        if (Overlap(object, region)) {
          return Status(ErrorCode::kInvalidArgument);
        }
      }
    }
  }
  return Status::Ok();
}

template <typename T>
void StartReport(const ReferenceLapackProvider& provider, const Call<T>& call,
                 LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  const auto name = Native<T>::kNames[static_cast<std::size_t>(call.operation)];
  std::copy(name.begin(), name.end(), report.routine.begin());
}

template <typename T>
extent_t RhsColumns(const Call<T>& call) {
  return call.rhs ? call.rhs->columns() : 0;
}

template <typename T>
bool Empty(const Call<T>& call) {
  return call.matrix.columns() == 0 ||
         (call.rhs && (call.matrix.rows() == 0 || call.rhs->columns() == 0));
}

template <typename T>
extent_t ForeignLeadingDimension(const Call<T>& call) {
  // Actual zero-row GEQP3 forms A(1,j) even for zero-length SWAP. An explicit
  // n-scalar caller surrogate with LDA=1 keeps every such address in backing.
  return call.operation == Operation::kGeqp3 && call.matrix.rows() == 0
             ? 1
             : internal_lapack_layout::LeadingDimension(call.matrix);
}

template <typename T>
Status ValidateOperands(const ReferenceLapackProvider& provider,
                        const Call<T>& call) {
  for (const auto storage : Operands(call)) {
    if (!provider.context().CanAccess(storage.space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  if (call.pivots.size() != call.matrix.columns() ||
      (call.tau && call.tau->size() !=
                       std::min(call.matrix.rows(), call.matrix.columns())) ||
      (call.rhs && call.rhs->rows() !=
                       std::max(call.matrix.rows(), call.matrix.columns()))) {
    return Status(ErrorCode::kShape);
  }
  if (call.pivots.increment() != 1 ||
      (call.tau && call.tau->increment() != 1) || !std::isfinite(call.rcond)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return CheckDisjoint(Operands(call));
}

template <typename Real>
std::int64_t ScalarBits(Real value) {
  if constexpr (std::same_as<Real, float>) {
    return std::bit_cast<std::int32_t>(value);
  } else {
    return std::bit_cast<std::int64_t>(value);
  }
}

template <typename T>
Status PlanRegions(const Call<T>& call, LapackWorkspacePlan& plan) {
  const bool empty = Empty(call);
  const extent_t n = call.matrix.columns();
  const extent_t k = std::min(call.matrix.rows(), n);
  const extent_t integers = empty ? 0 : n;
  const extent_t conversion =
      call.operation == Operation::kGeqp3 ? n : integers;
  const extent_t reals =
      DenseBlasComplex<T> && call.matrix.rows() != 0 ? 2 * integers : 0;
  plan.regions[kInteger] = {integers, integers, sizeof(lapack_int),
                            alignof(lapack_int)};
  plan.regions[kPermutation] = {conversion, conversion, sizeof(index_t),
                                alignof(index_t)};
  plan.regions[kReal] = {reals, reals, sizeof(DenseBlasRealType<T>),
                         alignof(DenseBlasRealType<T>)};
  extent_t packing = 0;
  if (!empty && call.matrix.rows() == 0) {
    packing = n;
  } else if (!empty && call.matrix.layout() == DenseBlasLayout::kRowMajor) {
    const auto count = internal_lapack_rank_revealing::AppendPacking(
        0, call.matrix.rows(), n, sizeof(T));
    if (!count.ok()) {
      return count.status();
    }
    packing = *count;
  }
  const extent_t output_rows = call.rhs ? call.rhs->rows() : k;
  const auto total = internal_lapack_rank_revealing::AppendPacking(
      packing, empty ? 0 : output_rows, call.rhs ? call.rhs->columns() : 1,
      sizeof(T));
  if (!total.ok()) {
    return total.status();
  }
  plan.regions[kPacking] = {*total, *total, sizeof(T), alignof(T)};
  return internal_lapack_layout::CheckTotal(plan);
}

template <typename T>
Result<LapackWorkspacePlan> Plan(const ReferenceLapackProvider& provider,
                                 const Call<T>& call) {
  const Status valid = ValidateOperands(provider, call);
  if (!valid.ok()) {
    return valid;
  }
  const auto counts =
      internal_lapack_rank_revealing::QueryCounts<DenseBlasRealType<T>>(
          call.operation, call.matrix.rows(), call.matrix.columns(),
          RhsColumns(call), ForeignLeadingDimension(call), DenseBlasComplex<T>,
          kIntegerLimit);
  if (!counts.ok()) {
    return counts.status();
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kNames[static_cast<std::size_t>(call.operation)],
      Native<T>::kKind,
      std::array{call.matrix.rows(), call.matrix.columns(), RhsColumns(call),
                 ForeignLeadingDimension(call)},
      std::array<std::int64_t, 9>{
          static_cast<std::int64_t>(call.matrix.layout()),
          call.rhs ? static_cast<std::int64_t>(call.rhs->layout()) : -1,
          call.matrix.leading_dimension(),
          call.rhs ? call.rhs->leading_dimension() : 0, call.pivots.increment(),
          call.tau ? call.tau->increment() : 0, ScalarBits(call.rcond),
          counts->minimum, counts->preferred},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  plan.regions[kScalar] = {counts->minimum, counts->preferred, sizeof(T),
                           alignof(T)};
  const Status regions = PlanRegions(call, plan);
  if (!regions.ok()) {
    return regions;
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
    if (!provider.context().CanAccess(workspace.regions[i].space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  return ValidateLapackWorkspace(supplied, expected.identity, workspace,
                                 operands);
}

Status InterpretInfo(lapack_int info, bool query, LapackReport& report) {
  report.native_info = info;
  if (info == 0) {
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = query ? LapackOutputValidity::kUnchanged
                                   : LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  report.outcome = info < 0 ? LapackOutcome::kProviderArgument
                            : LapackOutcome::kPartialResult;
  report.output_validity = query ? LapackOutputValidity::kUnchanged
                                 : LapackOutputValidity::kUnusable;
  if (info < 0 && info != std::numeric_limits<lapack_int>::min()) {
    report.native_argument = -static_cast<std::int64_t>(info);
  }
  return Status(ErrorCode::kProvider);
}

template <typename T>
lapack_int Invoke(const Call<T>& call, T* matrix, T* rhs, lapack_int* pivots,
                  T* tau, lapack_int& rank, T* work, lapack_int lwork,
                  DenseBlasRealType<T>* rwork) {
  T dummy_a{};
  T dummy_b{};
  T dummy_tau{};
  lapack_int dummy_pivot = 0;
  DenseBlasRealType<T> dummy_real{};
  return Native<T>::Invoke(
      call.operation, static_cast<lapack_int>(call.matrix.rows()),
      static_cast<lapack_int>(call.matrix.columns()),
      static_cast<lapack_int>(RhsColumns(call)),
      matrix == nullptr ? &dummy_a : matrix,
      static_cast<lapack_int>(ForeignLeadingDimension(call)),
      rhs == nullptr ? &dummy_b : rhs,
      static_cast<lapack_int>(std::max<extent_t>(
          1, std::max(call.matrix.rows(), call.matrix.columns()))),
      pivots == nullptr ? &dummy_pivot : pivots,
      tau == nullptr ? &dummy_tau : tau, call.rcond, rank, work, lwork,
      rwork == nullptr ? &dummy_real : rwork);
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Call<T>& call, LapackReport& report) {
  const Status metadata =
      CheckMetadata(std::array{ObjectStorage(provider), ObjectStorage(report)},
                    Operands(call), nullptr);
  if (!metadata.ok()) {
    return metadata;
  }
  StartReport(provider, call, report);
  const auto plan = Plan(provider, call);
  if (!plan.ok()) {
    return plan.status();
  }
  const auto counts =
      internal_lapack_rank_revealing::QueryCounts<DenseBlasRealType<T>>(
          call.operation, call.matrix.rows(), call.matrix.columns(),
          RhsColumns(call), ForeignLeadingDimension(call), DenseBlasComplex<T>,
          kIntegerLimit);
  T value{};
  lapack_int rank = 0;
  report.called_provider = true;
  const auto info = Invoke(
      call, call.matrix.data(), call.rhs ? call.rhs->data() : nullptr, nullptr,
      call.tau ? call.tau->data() : nullptr, rank, &value, -1, nullptr);
  const Status status = InterpretInfo(info, true, report);
  if (!status.ok()) {
    return status;
  }
  const auto checked =
      CheckedLapackQueryEntries(static_cast<double>(std::real(value)),
                                provider.identity().integer_abi, sizeof(T));
  if (!checked.ok() || std::imag(value) != 0 ||
      std::real(value) !=
          static_cast<DenseBlasRealType<T>>(counts->returned_preferred)) {
    report.outcome = LapackOutcome::kPartialResult;
    return checked.ok() ? Status(ErrorCode::kProvider) : checked.status();
  }
  return *plan;
}

template <typename T>
T& Entry(DenseBlasMatrixView<T> matrix, extent_t row, extent_t column) {
  const auto offset = matrix.layout() == DenseBlasLayout::kColumnMajor
                          ? column * matrix.leading_dimension() + row
                          : row * matrix.leading_dimension() + column;
  return matrix.data()[offset];
}

template <typename T>
bool MatrixIsZero(const Call<T>& call) {
  for (extent_t j = 0; j < call.matrix.columns(); ++j) {
    for (extent_t i = 0; i < call.matrix.rows(); ++i) {
      if (Entry(call.matrix, i, j) != T{}) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
void EmptyExecution(const Call<T>& call, LapackReport& report) {
  if (call.operation == Operation::kGeqp3) {
    // Only n=0 is local: there are no column outputs to substitute.
    report.factor_family = LapackFactorFamily::kColumnPivotedQr;
    report.outcome = LapackOutcome::kSuccess;
  } else {
    *call.rank = 0;
    report.outcome = LapackOutcome::kRankDecision;
  }
  report.output_validity = LapackOutputValidity::kComplete;
}

// Validate bounds before using any foreign index as an ASC array subscript.
// The caller conversion region doubles as an O(n) uniqueness bitmap.
Status StagePermutation(const lapack_int* pivots, extent_t n,
                        index_t* conversion) {
  for (extent_t j = 0; j < n; ++j) {
    if (pivots[j] < 1 || pivots[j] > n) {
      return Status(ErrorCode::kProvider);
    }
    conversion[j] = 0;
  }
  for (extent_t j = 0; j < n; ++j) {
    if (conversion[pivots[j] - 1] != 0) {
      return Status(ErrorCode::kProvider);
    }
    conversion[pivots[j] - 1] = 1;
  }
  for (extent_t j = 0; j < n; ++j) {
    conversion[j] = pivots[j];
  }
  return Status::Ok();
}

template <typename T>
void Publish(const Call<T>& call, const T* matrix, const T* output,
             const index_t* conversion, bool permutation, lapack_int rank,
             LapackReport& report) {
  internal_lapack_layout::Unpack(matrix, call.matrix);
  if (permutation) {
    for (extent_t j = 0; j < call.pivots.size(); ++j) {
      call.pivots.data()[j] = conversion[j];
    }
  }
  if (call.tau) {
    for (extent_t j = 0; j < call.tau->size(); ++j) {
      call.tau->data()[j] = output[j];
    }
    report.factor_family = LapackFactorFamily::kColumnPivotedQr;
  } else if (call.rhs) {
    for (extent_t j = 0; j < call.rhs->columns(); ++j) {
      for (extent_t i = 0; i < call.matrix.columns(); ++i) {
        Entry(*call.rhs, i, j) = output[j * call.rhs->rows() + i];
      }
    }
    *call.rank = rank;
    report.outcome = LapackOutcome::kRankDecision;
  }
}

template <typename T>
Status Compute(const Call<T>& call, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  auto* conversion =
      static_cast<index_t*>(workspace.regions[kPermutation].data());
  if (Empty(call)) {
    EmptyExecution(call, report);
    return Status::Ok();
  }
  // Only GELSY's all-zero-A early exit leaves JPVT as uncomputed flags.
  const bool permutation =
      call.operation == Operation::kGeqp3 || !MatrixIsZero(call);
  auto* cursor = static_cast<T*>(workspace.regions[kPacking].data());
  T* matrix = call.matrix.rows() == 0
                  ? cursor
                  : internal_lapack_layout::Pack(call.matrix, cursor);
  T* output = call.matrix.rows() == 0 ? nullptr : cursor;
  if (call.rhs) {
    for (extent_t j = 0; j < call.rhs->columns(); ++j) {
      for (extent_t i = 0; i < call.matrix.rows(); ++i) {
        output[j * call.rhs->rows() + i] = Entry(*call.rhs, i, j);
      }
    }
  }
  auto* pivots = static_cast<lapack_int*>(workspace.regions[kInteger].data());
  for (extent_t j = 0; j < call.pivots.size(); ++j) {
    pivots[j] = call.pivots.data()[j] == 0 ? 0 : 1;
  }
  lapack_int rank = -1;
  const auto lwork = static_cast<lapack_int>(std::min(
      workspace.regions[kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[kScalar].preferred_entries)));
  report.called_provider = true;
  Status status = InterpretInfo(
      Invoke(
          call, matrix, call.rhs ? output : nullptr, pivots,
          call.tau ? output : nullptr, rank,
          static_cast<T*>(workspace.regions[kScalar].data()), lwork,
          static_cast<DenseBlasRealType<T>*>(workspace.regions[kReal].data())),
      false, report);
  if (!status.ok()) {
    return status;
  }
  if ((call.rhs && (rank < 0 || rank > std::min(call.matrix.rows(),
                                                call.matrix.columns()))) ||
      (permutation &&
       !StagePermutation(pivots, call.pivots.size(), conversion).ok())) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  Publish(call, matrix, output, conversion, permutation, rank, report);
  return Status::Ok();
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider, const Call<T>& call,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  Status metadata =
      CheckMetadata(std::array{ObjectStorage(provider), ObjectStorage(plan),
                               ObjectStorage(workspace), ObjectStorage(report)},
                    Operands(call), &workspace);
  if (!metadata.ok()) {
    return metadata;
  }
  StartReport(provider, call, report);
  const auto expected = Plan(provider, call);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status =
      ValidatePlan(provider, *expected, plan, workspace, Operands(call));
  if (!status.ok()) {
    return status;
  }
  return Compute(call, plan, workspace, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryGeqp3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> column_pivots, DenseBlasVectorView<float> tau,
    LapackReport& report) {
  return Query(
      provider,
      Call<float>{Operation::kGeqp3, matrix, column_pivots, std::nullopt, tau},
      report);
}

Status Geqp3(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<index_t> column_pivots,
             DenseBlasVectorView<float> tau, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<float>{Operation::kGeqp3, matrix, column_pivots, std::nullopt, tau},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGelsyWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<index_t> column_pivots,
    float rcond, LapackReport& report) {
  return Query(provider,
               Call<float>{Operation::kGelsy, matrix, column_pivots, rhs,
                           std::nullopt, rcond},
               report);
}

Status Gelsy(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
             DenseBlasVectorView<index_t> column_pivots, float rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<float>{Operation::kGelsy, matrix, column_pivots, rhs,
                             std::nullopt, rcond, &rank},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqp3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> column_pivots, DenseBlasVectorView<double> tau,
    LapackReport& report) {
  return Query(
      provider,
      Call<double>{Operation::kGeqp3, matrix, column_pivots, std::nullopt, tau},
      report);
}

Status Geqp3(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<index_t> column_pivots,
             DenseBlasVectorView<double> tau, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<double>{Operation::kGeqp3, matrix, column_pivots, std::nullopt, tau},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGelsyWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasMatrixView<double> rhs, DenseBlasVectorView<index_t> column_pivots,
    double rcond, LapackReport& report) {
  return Query(provider,
               Call<double>{Operation::kGelsy, matrix, column_pivots, rhs,
                            std::nullopt, rcond},
               report);
}

Status Gelsy(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasMatrixView<double> rhs,
             DenseBlasVectorView<index_t> column_pivots, double rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<double>{Operation::kGelsy, matrix, column_pivots, rhs,
                              std::nullopt, rcond, &rank},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqp3Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> column_pivots,
    DenseBlasVectorView<std::complex<float>> tau, LapackReport& report) {
  return Query(provider,
               Call<std::complex<float>>{Operation::kGeqp3, matrix,
                                         column_pivots, std::nullopt, tau},
               report);
}

Status Geqp3(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> column_pivots,
             DenseBlasVectorView<std::complex<float>> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<float>>{Operation::kGeqp3, matrix,
                                           column_pivots, std::nullopt, tau},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGelsyWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasVectorView<index_t> column_pivots, float rcond,
    LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<float>>{Operation::kGelsy, matrix, column_pivots, rhs,
                                std::nullopt, rcond},
      report);
}

Status Gelsy(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasMatrixView<std::complex<float>> rhs,
             DenseBlasVectorView<index_t> column_pivots, float rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<float>>{Operation::kGelsy, matrix, column_pivots, rhs,
                                std::nullopt, rcond, &rank},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqp3Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> column_pivots,
    DenseBlasVectorView<std::complex<double>> tau, LapackReport& report) {
  return Query(provider,
               Call<std::complex<double>>{Operation::kGeqp3, matrix,
                                          column_pivots, std::nullopt, tau},
               report);
}

Status Geqp3(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> column_pivots,
             DenseBlasVectorView<std::complex<double>> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<double>>{Operation::kGeqp3, matrix,
                                            column_pivots, std::nullopt, tau},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGelsyWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasVectorView<index_t> column_pivots, double rcond,
    LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<double>>{Operation::kGelsy, matrix, column_pivots, rhs,
                                 std::nullopt, rcond},
      report);
}

Status Gelsy(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasMatrixView<std::complex<double>> rhs,
             DenseBlasVectorView<index_t> column_pivots, double rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<double>>{Operation::kGelsy, matrix, column_pivots, rhs,
                                 std::nullopt, rcond, &rank},
      plan, workspace, report);
}

}  // namespace asc
