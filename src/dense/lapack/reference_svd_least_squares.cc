#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_svd_least_squares.h"
#include "internal_layout.h"
#include "internal_least_squares_counts.h"
#include "internal_svd_least_squares_counts.h"
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
using internal_lapack_svd_least_squares::Counts;
using internal_lapack_svd_least_squares::Operation;
constexpr extent_t kIntegerLimit = std::numeric_limits<lapack_int>::max();
constexpr std::size_t kScalar =
    static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr std::size_t kReal =
    static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr std::size_t kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr std::size_t kPacking =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);
constexpr std::size_t kStaging =
    static_cast<std::size_t>(LapackWorkspaceKind::kScratch);

template <typename T>
using Real = DenseBlasRealType<T>;

template <typename T>
struct Call {
  Operation operation;
  DenseBlasMatrixView<T> matrix;
  DenseBlasMatrixView<T> rhs;
  DenseBlasVectorView<Real<T>> singular_values;
  Real<T> rcond;
};

template <typename T>
std::string_view Name(Operation operation) {
  if constexpr (std::is_same_v<T, float>) {
    return operation == Operation::kGelss ? "sgelss" : "sgelsd";
  } else if constexpr (std::is_same_v<T, double>) {
    return operation == Operation::kGelss ? "dgelss" : "dgelsd";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return operation == Operation::kGelss ? "cgelss" : "cgelsd";
  } else {
    return operation == Operation::kGelss ? "zgelss" : "zgelsd";
  }
}

template <typename T>
constexpr LapackScalarKind Kind() {
  if constexpr (std::is_same_v<T, float>) {
    return LapackScalarKind::kF32;
  } else if constexpr (std::is_same_v<T, double>) {
    return LapackScalarKind::kF64;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return LapackScalarKind::kC64;
  } else {
    return LapackScalarKind::kC128;
  }
}

template <typename T>
lapack_int Invoke(const Call<T>& call, T* a, T* b, Real<T>* s, T* work,
                  lapack_int lwork, Real<T>* rwork, lapack_int* iwork,
                  lapack_int& rank) {
  const lapack_int m = static_cast<lapack_int>(call.matrix.rows());
  const lapack_int n = static_cast<lapack_int>(call.matrix.columns());
  const lapack_int nrhs = static_cast<lapack_int>(call.rhs.columns());
  const lapack_int lda = static_cast<lapack_int>(
      internal_lapack_layout::LeadingDimension(call.matrix));
  const lapack_int ldb =
      static_cast<lapack_int>(std::max<extent_t>(1, call.rhs.rows()));
  lapack_int info = 0;
  if constexpr (std::is_same_v<T, float>) {
    if (call.operation == Operation::kGelsd) {
      LAPACK_sgelsd(&m, &n, &nrhs, a, &lda, b, &ldb, s, &call.rcond, &rank,
                    work, &lwork, iwork, &info);
    } else {
      LAPACK_sgelss(&m, &n, &nrhs, a, &lda, b, &ldb, s, &call.rcond, &rank,
                    work, &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (call.operation == Operation::kGelsd) {
      LAPACK_dgelsd(&m, &n, &nrhs, a, &lda, b, &ldb, s, &call.rcond, &rank,
                    work, &lwork, iwork, &info);
    } else {
      LAPACK_dgelss(&m, &n, &nrhs, a, &lda, b, &ldb, s, &call.rcond, &rank,
                    work, &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (call.operation == Operation::kGelsd) {
      LAPACK_cgelsd(&m, &n, &nrhs, a, &lda, b, &ldb, s, &call.rcond, &rank,
                    work, &lwork, rwork, iwork, &info);
    } else {
      LAPACK_cgelss(&m, &n, &nrhs, a, &lda, b, &ldb, s, &call.rcond, &rank,
                    work, &lwork, rwork, &info);
    }
  } else {
    if (call.operation == Operation::kGelsd) {
      LAPACK_zgelsd(&m, &n, &nrhs, a, &lda, b, &ldb, s, &call.rcond, &rank,
                    work, &lwork, rwork, iwork, &info);
    } else {
      LAPACK_zgelss(&m, &n, &nrhs, a, &lda, b, &ldb, s, &call.rcond, &rank,
                    work, &lwork, rwork, &info);
    }
  }
  return info;
}

bool Overlap(ConstMemoryView a, ConstMemoryView b) {
  if (a.size() == 0 || b.size() == 0) {
    return false;
  }
  const auto first = reinterpret_cast<std::uintptr_t>(a.data());
  const auto second = reinterpret_cast<std::uintptr_t>(b.data());
  return first <= second ? second - first < a.size()
                         : first - second < b.size();
}

template <typename T>
ConstMemoryView Object(const T& value) {
  return {&value, sizeof(value), MemorySpace::kHost};
}

template <typename T>
auto Operands(const Call<T>& call) {
  return std::array<ConstMemoryView, 3>{
      call.matrix.reachable_storage(), call.rhs.reachable_storage(),
      call.singular_values.reachable_storage()};
}

Status Disjoint(std::span<const ConstMemoryView> objects) {
  for (std::size_t i = 0; i < objects.size(); ++i) {
    for (std::size_t j = i + 1; j < objects.size(); ++j) {
      if (Overlap(objects[i], objects[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

Status Metadata(std::span<const ConstMemoryView> metadata,
                std::span<const ConstMemoryView> operands,
                const LapackWorkspace* workspace) {
  Status status = Disjoint(metadata);
  if (!status.ok()) {
    return status;
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
  const auto name = Name<T>(call.operation);
  std::copy(name.begin(), name.end(), report.routine.begin());
}

template <typename T>
Result<Counts> Validate(const ReferenceLapackProvider& provider,
                        const Call<T>& call) {
  const extent_t k = std::min(call.matrix.rows(), call.matrix.columns());
  if (call.rhs.rows() != std::max(call.matrix.rows(), call.matrix.columns()) ||
      call.singular_values.size() != k) {
    return Status(ErrorCode::kShape);
  }
  if (call.singular_values.increment() != 1 || !std::isfinite(call.rcond)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  for (const auto operand : Operands(call)) {
    if (!provider.context().CanAccess(operand.space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  Status alias = Disjoint(Operands(call));
  if (!alias.ok()) {
    return alias;
  }
  return internal_lapack_svd_least_squares::QueryCounts<Real<T>>(
      call.operation, call.matrix.rows(), call.matrix.columns(),
      call.rhs.columns(), internal_lapack_layout::LeadingDimension(call.matrix),
      DenseBlasComplex<T>, kIntegerLimit);
}

template <typename T>
std::int64_t CutoffBits(T cutoff) {
  if constexpr (sizeof(T) == sizeof(std::int32_t)) {
    return std::bit_cast<std::int32_t>(cutoff);
  } else {
    return std::bit_cast<std::int64_t>(cutoff);
  }
}

template <typename T>
Result<LapackWorkspacePlan> Plan(const ReferenceLapackProvider& provider,
                                 const Call<T>& call, const Counts& counts) {
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(call.operation), Kind<T>(),
      std::array{call.matrix.rows(), call.matrix.columns(), call.rhs.columns(),
                 internal_lapack_layout::LeadingDimension(call.matrix),
                 std::max<extent_t>(1, call.rhs.rows()),
                 call.singular_values.size()},
      std::array<std::int64_t, 11>{
          static_cast<std::int64_t>(call.matrix.layout()),
          static_cast<std::int64_t>(call.rhs.layout()),
          call.matrix.leading_dimension(), call.rhs.leading_dimension(),
          call.singular_values.increment(), CutoffBits(call.rcond),
          counts.minimum, counts.preferred, counts.scalar_query, counts.real,
          counts.integer},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  plan.regions[kScalar] = {counts.minimum, counts.preferred, sizeof(T),
                           alignof(T)};
  plan.regions[kReal] = {counts.real, counts.real, sizeof(Real<T>),
                         alignof(Real<T>)};
  plan.regions[kInteger] = {counts.integer, counts.integer, sizeof(lapack_int),
                            alignof(lapack_int)};
  const extent_t k = call.singular_values.size();
  plan.regions[kStaging] = {k, k, sizeof(Real<T>), alignof(Real<T>)};
  extent_t packing = 0;
  if (k != 0) {
    if (call.matrix.layout() == DenseBlasLayout::kRowMajor) {
      const auto added = internal_lapack_least_squares::AppendPacking(
          0, call.matrix.rows(), call.matrix.columns(), sizeof(T));
      if (!added.ok()) {
        return added.status();
      }
      packing = *added;
    }
    const auto added = internal_lapack_least_squares::AppendPacking(
        packing, call.rhs.rows(), std::max<extent_t>(1, call.rhs.columns()),
        sizeof(T));
    if (!added.ok()) {
      return added.status();
    }
    packing = *added;
  }
  plan.regions[kPacking] = {packing, packing, sizeof(T), alignof(T)};
  Status total = internal_lapack_layout::CheckTotal(plan);
  if (!total.ok()) {
    return total;
  }
  return plan;
}

Status Info(lapack_int info, bool query, extent_t k, LapackReport& report) {
  report.native_info = info;
  if (info == 0) {
    report.outcome =
        query ? LapackOutcome::kSuccess : LapackOutcome::kRankDecision;
    report.output_validity = query ? LapackOutputValidity::kUnchanged
                                   : LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  if (info > 0 && !query && info < k) {
    report.outcome = LapackOutcome::kNonconvergence;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
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
Status QueryForeign(const ReferenceLapackProvider& provider,
                    const Call<T>& call, const Counts& counts,
                    LapackReport& report) {
  T dummy_a{};
  T dummy_b{};
  Real<T> dummy_s{};
  T work{};
  Real<T> real_work{};
  lapack_int integer_work = 0;
  lapack_int rank = -1;
  report.called_provider = true;
  const auto info = Invoke(
      call, call.matrix.data() == nullptr ? &dummy_a : call.matrix.data(),
      call.rhs.data() == nullptr ? &dummy_b : call.rhs.data(),
      call.singular_values.data() == nullptr ? &dummy_s
                                             : call.singular_values.data(),
      &work, -1, &real_work, &integer_work, rank);
  Status status = Info(info, true, call.singular_values.size(), report);
  if (!status.ok()) {
    return status;
  }
  const auto scalar =
      CheckedLapackQueryEntries(static_cast<double>(std::real(work)),
                                provider.identity().integer_abi, sizeof(T));
  if (!scalar.ok() || std::imag(work) != 0 ||
      std::real(work) != static_cast<Real<T>>(counts.scalar_query)) {
    report.outcome = LapackOutcome::kPartialResult;
    return scalar.ok() ? Status(ErrorCode::kProvider) : scalar.status();
  }
  if (call.operation == Operation::kGelsd && integer_work != counts.integer) {
    report.outcome = LapackOutcome::kPartialResult;
    return Status(ErrorCode::kProvider);
  }
  if constexpr (DenseBlasComplex<T>) {
    if (call.operation == Operation::kGelsd) {
      const auto real = CheckedLapackQueryEntries(
          static_cast<double>(real_work), provider.identity().integer_abi,
          sizeof(Real<T>));
      if (!real.ok() || real_work != static_cast<Real<T>>(counts.real_query)) {
        report.outcome = LapackOutcome::kPartialResult;
        return real.ok() ? Status(ErrorCode::kProvider) : real.status();
      }
    }
  }
  return Status::Ok();
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Call<T>& call, LapackReport& report) {
  Status status = Metadata(std::array{Object(provider), Object(report)},
                           Operands(call), nullptr);
  if (!status.ok()) {
    return status;
  }
  StartReport(provider, call, report);
  const auto counts = Validate(provider, call);
  if (!counts.ok()) {
    return counts.status();
  }
  const auto plan = Plan(provider, call, *counts);
  if (!plan.ok()) {
    return plan.status();
  }
  status = QueryForeign(provider, call, *counts, report);
  return status.ok() ? plan : Result<LapackWorkspacePlan>(status);
}

template <typename T>
T& Entry(DenseBlasMatrixView<T> matrix, extent_t i, extent_t j) {
  return matrix.data()[matrix.layout() == DenseBlasLayout::kColumnMajor
                           ? j * matrix.leading_dimension() + i
                           : i * matrix.leading_dimension() + j];
}

template <typename T>
Result<bool> CheckInput(const Call<T>& call) {
  bool zero = true;
  for (extent_t j = 0; j < call.matrix.columns(); ++j) {
    for (extent_t i = 0; i < call.matrix.rows(); ++i) {
      const T value = Entry(call.matrix, i, j);
      if (!std::isfinite(std::real(value)) ||
          !std::isfinite(std::imag(value))) {
        return Status(ErrorCode::kNumerical);
      }
      zero = zero && value == T{};
    }
  }
  if (!zero) {
    for (extent_t j = 0; j < call.rhs.columns(); ++j) {
      for (extent_t i = 0; i < call.matrix.rows(); ++i) {
        const T value = Entry(call.rhs, i, j);
        if (!std::isfinite(std::real(value)) ||
            !std::isfinite(std::imag(value))) {
          return Status(ErrorCode::kNumerical);
        }
      }
    }
  }
  return zero;
}

Status ValidatePlan(const ReferenceLapackProvider& provider,
                    const LapackWorkspacePlan& expected,
                    const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace,
                    std::span<const ConstMemoryView> operands) {
  if (plan.total_byte_limit != expected.total_byte_limit) {
    return Status(ErrorCode::kInvalidState);
  }
  for (std::size_t i = 0; i < plan.regions.size(); ++i) {
    const auto& first = plan.regions[i];
    const auto& second = expected.regions[i];
    if (first.minimum_entries != second.minimum_entries ||
        first.preferred_entries != second.preferred_entries ||
        first.entry_bytes != second.entry_bytes ||
        first.alignment != second.alignment) {
      return Status(ErrorCode::kInvalidState);
    }
  }
  return internal_lapack_workspace::Validate(provider, plan, expected.identity,
                                             workspace, operands);
}

template <typename T>
void Publish(const Call<T>& call, const T* a, const T* b, const Real<T>* s,
             extent_t rhs_rows) {
  internal_lapack_layout::Unpack(a, call.matrix);
  for (extent_t j = 0; j < call.rhs.columns(); ++j) {
    for (extent_t i = 0; i < rhs_rows; ++i) {
      Entry(call.rhs, i, j) = b[j * call.rhs.rows() + i];
    }
  }
  std::copy_n(s, call.singular_values.size(), call.singular_values.data());
}

template <typename T>
Status Run(const Call<T>& call, index_t& rank, const LapackWorkspacePlan& plan,
           const LapackWorkspace& workspace, bool zero, LapackReport& report) {
  auto* cursor = static_cast<T*>(workspace.regions[kPacking].data());
  T* a = internal_lapack_layout::Pack(call.matrix, cursor);
  T* b = cursor;
  if (!zero) {
    for (extent_t j = 0; j < call.rhs.columns(); ++j) {
      for (extent_t i = 0; i < call.matrix.rows(); ++i) {
        b[j * call.rhs.rows() + i] = Entry(call.rhs, i, j);
      }
    }
  }
  auto* s = static_cast<Real<T>*>(workspace.regions[kStaging].data());
  lapack_int native_rank = -1;
  const auto lwork = static_cast<lapack_int>(std::min(
      workspace.regions[kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[kScalar].preferred_entries)));
  report.called_provider = true;
  Status status = Info(
      Invoke(call, a, b, s, static_cast<T*>(workspace.regions[kScalar].data()),
             lwork, static_cast<Real<T>*>(workspace.regions[kReal].data()),
             static_cast<lapack_int*>(workspace.regions[kInteger].data()),
             native_rank),
      false, call.singular_values.size(), report);
  if (status.ok()) {
    if (native_rank < 0 || native_rank > call.singular_values.size()) {
      report.outcome = LapackOutcome::kPartialResult;
      report.output_validity = LapackOutputValidity::kUnusable;
      return Status(ErrorCode::kProvider);
    }
    const extent_t rows = call.matrix.rows() >= call.matrix.columns() &&
                                  native_rank == call.matrix.columns()
                              ? call.matrix.rows()
                              : call.matrix.columns();
    Publish(call, a, b, s, rows);
    rank = native_rank;
  } else if (report.outcome == LapackOutcome::kNonconvergence) {
    Publish(call, a, b, s, call.matrix.rows());
  }
  return status;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider, const Call<T>& call,
               index_t& rank, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  Status status =
      Metadata(std::array{Object(provider), Object(rank), Object(plan),
                          Object(workspace), Object(report)},
               Operands(call), &workspace);
  if (!status.ok()) {
    return status;
  }
  StartReport(provider, call, report);
  const auto counts = Validate(provider, call);
  if (!counts.ok()) {
    return counts.status();
  }
  const auto expected = Plan(provider, call, *counts);
  if (!expected.ok()) {
    return expected.status();
  }
  status = ValidatePlan(provider, *expected, plan, workspace, Operands(call));
  if (!status.ok()) {
    return status;
  }
  if (call.singular_values.size() == 0) {
    rank = 0;
    report.outcome = LapackOutcome::kRankDecision;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  const auto zero = CheckInput(call);
  if (!zero.ok()) {
    return zero.status();
  }
  if (call.operation == Operation::kGelsd) {
    const Status tree =
        internal_lapack_svd_least_squares::DivideTreeAdmission<Real<T>>(
            call.singular_values.size());
    if ((call.rhs.columns() == 0 || !tree.ok()) && !*zero) {
      return Status(ErrorCode::kUnsupported);
    }
  }
  return Run(call, rank, plan, workspace, *zero, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryGelssWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<float> singular_values,
    float rcond, LapackReport& report) {
  return Query(
      provider,
      Call<float>{Operation::kGelss, matrix, rhs, singular_values, rcond},
      report);
}
Status Gelss(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
             DenseBlasVectorView<float> singular_values, float rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<float>{Operation::kGelss, matrix, rhs, singular_values, rcond}, rank,
      plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGelssWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasMatrixView<double> rhs,
    DenseBlasVectorView<double> singular_values, double rcond,
    LapackReport& report) {
  return Query(
      provider,
      Call<double>{Operation::kGelss, matrix, rhs, singular_values, rcond},
      report);
}
Status Gelss(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasMatrixView<double> rhs,
             DenseBlasVectorView<double> singular_values, double rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<double>{Operation::kGelss, matrix, rhs, singular_values, rcond},
      rank, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGelssWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasVectorView<float> singular_values, float rcond,
    LapackReport& report) {
  return Query(provider,
               Call<std::complex<float>>{Operation::kGelss, matrix, rhs,
                                         singular_values, rcond},
               report);
}
Status Gelss(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasMatrixView<std::complex<float>> rhs,
             DenseBlasVectorView<float> singular_values, float rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<float>>{Operation::kGelss, matrix, rhs,
                                           singular_values, rcond},
                 rank, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGelssWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasVectorView<double> singular_values, double rcond,
    LapackReport& report) {
  return Query(provider,
               Call<std::complex<double>>{Operation::kGelss, matrix, rhs,
                                          singular_values, rcond},
               report);
}
Status Gelss(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasMatrixView<std::complex<double>> rhs,
             DenseBlasVectorView<double> singular_values, double rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<double>>{Operation::kGelss, matrix, rhs,
                                            singular_values, rcond},
                 rank, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGelsdWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<float> singular_values,
    float rcond, LapackReport& report) {
  return Query(
      provider,
      Call<float>{Operation::kGelsd, matrix, rhs, singular_values, rcond},
      report);
}
Status Gelsd(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
             DenseBlasVectorView<float> singular_values, float rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<float>{Operation::kGelsd, matrix, rhs, singular_values, rcond}, rank,
      plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGelsdWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasMatrixView<double> rhs,
    DenseBlasVectorView<double> singular_values, double rcond,
    LapackReport& report) {
  return Query(
      provider,
      Call<double>{Operation::kGelsd, matrix, rhs, singular_values, rcond},
      report);
}
Status Gelsd(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasMatrixView<double> rhs,
             DenseBlasVectorView<double> singular_values, double rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<double>{Operation::kGelsd, matrix, rhs, singular_values, rcond},
      rank, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGelsdWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasVectorView<float> singular_values, float rcond,
    LapackReport& report) {
  return Query(provider,
               Call<std::complex<float>>{Operation::kGelsd, matrix, rhs,
                                         singular_values, rcond},
               report);
}
Status Gelsd(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasMatrixView<std::complex<float>> rhs,
             DenseBlasVectorView<float> singular_values, float rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<float>>{Operation::kGelsd, matrix, rhs,
                                           singular_values, rcond},
                 rank, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGelsdWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasVectorView<double> singular_values, double rcond,
    LapackReport& report) {
  return Query(provider,
               Call<std::complex<double>>{Operation::kGelsd, matrix, rhs,
                                          singular_values, rcond},
               report);
}
Status Gelsd(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasMatrixView<std::complex<double>> rhs,
             DenseBlasVectorView<double> singular_values, double rcond,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<double>>{Operation::kGelsd, matrix, rhs,
                                            singular_values, rcond},
                 rank, plan, workspace, report);
}

}  // namespace asc
