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
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_qr.h"
#include "internal_layout.h"
#include "internal_qr_counts.h"
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
using internal_lapack_qr::Operation;
constexpr std::size_t kScalar =
    static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr std::size_t kPacking =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);
constexpr extent_t kIntegerLimit = std::numeric_limits<lapack_int>::max();

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF32;
  static constexpr std::array<std::string_view, 4> kNames{"sgeqrf", "sgeqr2",
                                                          "sorgqr", "sormqr"};
  static lapack_int Factor(Operation operation, lapack_int m, lapack_int n,
                           float* a, lapack_int lda, float* tau, float* work,
                           lapack_int lwork) {
    lapack_int info = 0;
    if (operation == Operation::kGeqr2) {
      LAPACK_sgeqr2(&m, &n, a, &lda, tau, work, &info);
    } else {
      LAPACK_sgeqrf(&m, &n, a, &lda, tau, work, &lwork, &info);
    }
    return info;
  }
  static lapack_int Generate(lapack_int m, lapack_int n, lapack_int k, float* a,
                             lapack_int lda, const float* tau, float* work,
                             lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_sorgqr(&m, &n, &k, a, &lda, tau, work, &lwork, &info);
    return info;
  }
  static lapack_int Apply(char side, char transpose, lapack_int m, lapack_int n,
                          lapack_int k, const float* a, lapack_int lda,
                          const float* tau, float* matrix, lapack_int ldc,
                          float* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_sormqr(&side, &transpose, &m, &n, &k, a, &lda, tau, matrix, &ldc,
                  work, &lwork, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF64;
  static constexpr std::array<std::string_view, 4> kNames{"dgeqrf", "dgeqr2",
                                                          "dorgqr", "dormqr"};
  static lapack_int Factor(Operation operation, lapack_int m, lapack_int n,
                           double* a, lapack_int lda, double* tau, double* work,
                           lapack_int lwork) {
    lapack_int info = 0;
    if (operation == Operation::kGeqr2) {
      LAPACK_dgeqr2(&m, &n, a, &lda, tau, work, &info);
    } else {
      LAPACK_dgeqrf(&m, &n, a, &lda, tau, work, &lwork, &info);
    }
    return info;
  }
  static lapack_int Generate(lapack_int m, lapack_int n, lapack_int k,
                             double* a, lapack_int lda, const double* tau,
                             double* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_dorgqr(&m, &n, &k, a, &lda, tau, work, &lwork, &info);
    return info;
  }
  static lapack_int Apply(char side, char transpose, lapack_int m, lapack_int n,
                          lapack_int k, const double* a, lapack_int lda,
                          const double* tau, double* matrix, lapack_int ldc,
                          double* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_dormqr(&side, &transpose, &m, &n, &k, a, &lda, tau, matrix, &ldc,
                  work, &lwork, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC64;
  static constexpr std::array<std::string_view, 4> kNames{"cgeqrf", "cgeqr2",
                                                          "cungqr", "cunmqr"};
  static lapack_int Factor(Operation operation, lapack_int m, lapack_int n,
                           std::complex<float>* a, lapack_int lda,
                           std::complex<float>* tau, std::complex<float>* work,
                           lapack_int lwork) {
    lapack_int info = 0;
    if (operation == Operation::kGeqr2) {
      LAPACK_cgeqr2(&m, &n, a, &lda, tau, work, &info);
    } else {
      LAPACK_cgeqrf(&m, &n, a, &lda, tau, work, &lwork, &info);
    }
    return info;
  }
  static lapack_int Generate(lapack_int m, lapack_int n, lapack_int k,
                             std::complex<float>* a, lapack_int lda,
                             const std::complex<float>* tau,
                             std::complex<float>* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_cungqr(&m, &n, &k, a, &lda, tau, work, &lwork, &info);
    return info;
  }
  static lapack_int Apply(char side, char transpose, lapack_int m, lapack_int n,
                          lapack_int k, const std::complex<float>* a,
                          lapack_int lda, const std::complex<float>* tau,
                          std::complex<float>* matrix, lapack_int ldc,
                          std::complex<float>* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_cunmqr(&side, &transpose, &m, &n, &k, a, &lda, tau, matrix, &ldc,
                  work, &lwork, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC128;
  static constexpr std::array<std::string_view, 4> kNames{"zgeqrf", "zgeqr2",
                                                          "zungqr", "zunmqr"};
  static lapack_int Factor(Operation operation, lapack_int m, lapack_int n,
                           std::complex<double>* a, lapack_int lda,
                           std::complex<double>* tau,
                           std::complex<double>* work, lapack_int lwork) {
    lapack_int info = 0;
    if (operation == Operation::kGeqr2) {
      LAPACK_zgeqr2(&m, &n, a, &lda, tau, work, &info);
    } else {
      LAPACK_zgeqrf(&m, &n, a, &lda, tau, work, &lwork, &info);
    }
    return info;
  }
  static lapack_int Generate(lapack_int m, lapack_int n, lapack_int k,
                             std::complex<double>* a, lapack_int lda,
                             const std::complex<double>* tau,
                             std::complex<double>* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_zungqr(&m, &n, &k, a, &lda, tau, work, &lwork, &info);
    return info;
  }
  static lapack_int Apply(char side, char transpose, lapack_int m, lapack_int n,
                          lapack_int k, const std::complex<double>* a,
                          lapack_int lda, const std::complex<double>* tau,
                          std::complex<double>* matrix, lapack_int ldc,
                          std::complex<double>* work, lapack_int lwork) {
    lapack_int info = 0;
    LAPACK_zunmqr(&side, &transpose, &m, &n, &k, a, &lda, tau, matrix, &ldc,
                  work, &lwork, &info);
    return info;
  }
};

template <typename T>
struct Call {
  Operation operation;
  DenseBlasMatrixView<T> matrix;
  DenseBlasMatrixView<const T> reflectors;
  DenseBlasVectorView<const T> tau;
  T* tau_output;
  DenseBlasSide side = DenseBlasSide::kLeft;
  DenseBlasTranspose transpose = DenseBlasTranspose::kNone;
};

template <typename T>
std::array<ConstMemoryView, 3> Operands(const Call<T>& call) {
  return {call.matrix.reachable_storage(), call.tau.reachable_storage(),
          call.operation == Operation::kApply
              ? call.reflectors.reachable_storage()
              : ConstMemoryView(nullptr, 0, MemorySpace::kHost)};
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
Status ValidateMatrix(const ReferenceLapackProvider& provider,
                      DenseBlasMatrixView<T> matrix) {
  if (!provider.context().CanAccess(matrix.memory_space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  for (const auto value : {matrix.rows(), matrix.columns(),
                           internal_lapack_layout::LeadingDimension(matrix)}) {
    if (value > kIntegerLimit) {
      return Status(ErrorCode::kOverflow);
    }
  }
  return Status::Ok();
}

template <typename T>
Status ValidateOperands(const ReferenceLapackProvider& provider,
                        const Call<T>& call) {
  Status status = ValidateMatrix(provider, call.matrix);
  if (!status.ok()) {
    return status;
  }
  if (!provider.context().CanAccess(call.tau.memory_space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (call.tau.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (call.tau.size() > kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  if (call.operation == Operation::kApply) {
    if (call.side != DenseBlasSide::kLeft &&
        call.side != DenseBlasSide::kRight) {
      return Status(ErrorCode::kInvalidArgument);
    }
    constexpr auto kAdjoint = DenseBlasComplex<T>
                                  ? DenseBlasTranspose::kConjugateTranspose
                                  : DenseBlasTranspose::kTranspose;
    if (call.transpose != DenseBlasTranspose::kNone &&
        call.transpose != kAdjoint) {
      return Status(ErrorCode::kInvalidArgument);
    }
    status = ValidateMatrix(provider, call.reflectors);
    if (!status.ok()) {
      return status;
    }
    const auto order = call.side == DenseBlasSide::kLeft
                           ? call.matrix.rows()
                           : call.matrix.columns();
    if (call.reflectors.rows() != order ||
        call.reflectors.columns() != call.tau.size()) {
      return Status(ErrorCode::kShape);
    }
  }
  return CheckDisjoint(Operands(call));
}

template <typename T>
Status AddPacking(DenseBlasMatrixView<T> matrix, LapackWorkspacePlan& plan) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return internal_lapack_layout::CheckTotal(plan);
  }
  auto& region = plan.regions[kPacking];
  const auto count = internal_lapack_qr::AppendPacking(
      region.minimum_entries, matrix.rows(), matrix.columns(), sizeof(T));
  if (!count.ok()) {
    return count.status();
  }
  region = {*count, *count, sizeof(T), alignof(T)};
  return internal_lapack_layout::CheckTotal(plan);
}

template <typename T>
Result<LapackWorkspacePlan> Plan(const ReferenceLapackProvider& provider,
                                 const Call<T>& call, extent_t preferred) {
  const Status valid = ValidateOperands(provider, call);
  if (!valid.ok()) {
    return valid;
  }
  const auto counts = internal_lapack_qr::QueryCounts(
      call.operation, call.matrix.rows(), call.matrix.columns(),
      call.tau.size(), call.side == DenseBlasSide::kLeft, kIntegerLimit);
  if (!counts.ok()) {
    return counts.status();
  }
  const auto guarded =
      call.operation == Operation::kGeqr2
          ? Result<extent_t>(counts->raw_preferred)
          : internal_lapack_qr::GuardQueryCapacity<DenseBlasRealType<T>>(
                counts->raw_preferred, kIntegerLimit);
  if (!guarded.ok()) {
    return guarded.status();
  }
  const extent_t expected =
      call.operation == Operation::kGeqr2 ? counts->raw_preferred : *guarded;
  if (preferred < 0) {
    preferred = expected;
  }
  if (preferred != expected) {
    return Status(ErrorCode::kInvalidState);
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kNames[static_cast<std::size_t>(call.operation)],
      Native<T>::kKind,
      std::array{call.matrix.rows(), call.matrix.columns(), call.tau.size(),
                 internal_lapack_layout::LeadingDimension(call.matrix),
                 call.reflectors.rows(), call.reflectors.columns(),
                 internal_lapack_layout::LeadingDimension(call.reflectors)},
      std::array<std::int64_t, 9>{
          static_cast<std::int64_t>(call.side),
          static_cast<std::int64_t>(call.transpose),
          static_cast<std::int64_t>(call.matrix.layout()),
          static_cast<std::int64_t>(call.reflectors.layout()),
          call.tau.increment(), counts->minimum, preferred,
          call.matrix.leading_dimension(), call.reflectors.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  plan.regions[kScalar] = {counts->minimum, preferred, sizeof(T), alignof(T)};
  Status packed = AddPacking(call.matrix, plan);
  if (packed.ok() && call.operation == Operation::kApply) {
    packed = AddPacking(call.reflectors, plan);
  }
  if (!packed.ok()) {
    return packed;
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
  if (info != 0) {
    report.outcome = info < 0 ? LapackOutcome::kProviderArgument
                              : LapackOutcome::kPartialResult;
    report.output_validity = query ? LapackOutputValidity::kUnchanged
                                   : LapackOutputValidity::kUnusable;
    if (info < 0 && info != std::numeric_limits<lapack_int>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = query ? LapackOutputValidity::kUnchanged
                                 : LapackOutputValidity::kComplete;
  return Status::Ok();
}

template <typename T>
lapack_int Invoke(const Call<T>& call, T* matrix, const T* reflectors, T* work,
                  lapack_int lwork) {
  const auto m = static_cast<lapack_int>(call.matrix.rows());
  const auto n = static_cast<lapack_int>(call.matrix.columns());
  const auto k = static_cast<lapack_int>(call.tau.size());
  const auto ldc = static_cast<lapack_int>(
      internal_lapack_layout::LeadingDimension(call.matrix));
  // Empty objects use local typed dummies; the pinned query paths read no
  // numeric entries. Nonempty calls always carry complete actual live arrays.
  T dummy{};
  if (matrix == nullptr) {
    matrix = &dummy;
  }
  const T* tau = call.tau.data() == nullptr ? &dummy : call.tau.data();
  if (call.operation == Operation::kGenerate) {
    return Native<T>::Generate(m, n, k, matrix, ldc, tau, work, lwork);
  }
  if (call.operation == Operation::kApply) {
    const char side = call.side == DenseBlasSide::kLeft ? 'L' : 'R';
    const char adjoint = DenseBlasComplex<T> ? 'C' : 'T';
    const char transpose =
        call.transpose == DenseBlasTranspose::kNone ? 'N' : adjoint;
    if (reflectors == nullptr) {
      reflectors = &dummy;
    }
    const auto lda = static_cast<lapack_int>(
        internal_lapack_layout::LeadingDimension(call.reflectors));
    return Native<T>::Apply(side, transpose, m, n, k, reflectors, lda, tau,
                            matrix, ldc, work, lwork);
  }
  T* tau_output = call.tau_output == nullptr ? &dummy : call.tau_output;
  return Native<T>::Factor(call.operation, m, n, matrix, ldc, tau_output, work,
                           lwork);
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
  const auto plan = Plan(provider, call, -1);
  if (!plan.ok()) {
    return plan.status();
  }
  if (call.operation == Operation::kGeqr2) {
    report.outcome = LapackOutcome::kSuccess;
    return *plan;
  }
  T query_value{};
  report.called_provider = true;
  const auto info = Invoke(call, call.matrix.data(), call.reflectors.data(),
                           &query_value, -1);
  const Status interpreted = InterpretInfo(info, true, report);
  if (!interpreted.ok()) {
    return interpreted;
  }
  const auto checked =
      CheckedLapackQueryEntries(static_cast<double>(std::real(query_value)),
                                provider.identity().integer_abi, sizeof(T));
  const auto preferred = plan->regions[kScalar].preferred_entries;
  if (!checked.ok() || std::imag(query_value) != 0 ||
      std::real(query_value) != static_cast<DenseBlasRealType<T>>(preferred) ||
      *checked < plan->regions[kScalar].minimum_entries) {
    report.outcome = LapackOutcome::kPartialResult;
    return checked.ok() ? Status(ErrorCode::kProvider) : checked.status();
  }
  return *plan;
}

template <typename T>
T* PackReflectors(DenseBlasMatrixView<T> matrix, extent_t k,
                  std::remove_const_t<T>*& cursor) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return matrix.data();
  }
  auto* result = cursor;
  for (extent_t column = 0; column < k; ++column) {
    for (extent_t row = column + 1; row < matrix.rows(); ++row) {
      result[column * matrix.rows() + row] =
          matrix.data()[row * matrix.leading_dimension() + column];
    }
  }
  cursor += matrix.rows() * matrix.columns();
  return result;
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
  const auto expected =
      Plan(provider, call, plan.regions[kScalar].preferred_entries);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status =
      ValidatePlan(provider, *expected, plan, workspace, Operands(call));
  if (!status.ok()) {
    return status;
  }
  if (call.matrix.rows() == 0 || call.matrix.columns() == 0 ||
      (call.operation == Operation::kApply && call.tau.size() == 0)) {
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    if (call.operation == Operation::kGeqrf ||
        call.operation == Operation::kGeqr2) {
      report.factor_family = LapackFactorFamily::kHouseholderQr;
    }
    return Status::Ok();
  }
  auto* cursor = static_cast<T*>(workspace.regions[kPacking].data());
  T* matrix = call.operation == Operation::kGenerate
                  ? PackReflectors(call.matrix, call.tau.size(), cursor)
                  : internal_lapack_layout::Pack(call.matrix, cursor);
  const T* reflectors =
      call.operation == Operation::kApply
          ? PackReflectors(call.reflectors, call.tau.size(), cursor)
          : call.reflectors.data();
  const auto lwork = static_cast<lapack_int>(std::min(
      workspace.regions[kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[kScalar].preferred_entries)));
  report.called_provider = true;
  status = InterpretInfo(
      Invoke(call, matrix, reflectors,
             static_cast<T*>(workspace.regions[kScalar].data()), lwork),
      false, report);
  if (status.ok()) {
    internal_lapack_layout::Unpack(matrix, call.matrix);
    if (call.operation == Operation::kGeqrf ||
        call.operation == Operation::kGeqr2) {
      report.factor_family = LapackFactorFamily::kHouseholderQr;
    }
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryGeqrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<float> tau, LapackReport& report) {
  return Query(provider,
               Call<float>{Operation::kGeqrf, matrix, matrix, tau, tau.data()},
               report);
}

Status Geqrf(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider, Call<float>{Operation::kGeqrf, matrix, matrix, tau, tau.data()},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqr2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<float> tau, LapackReport& report) {
  return Query(provider,
               Call<float>{Operation::kGeqr2, matrix, matrix, tau, tau.data()},
               report);
}

Status Geqr2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider, Call<float>{Operation::kGeqr2, matrix, matrix, tau, tau.data()},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryOrgqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const float> tau, LapackReport& report) {
  return Query(provider,
               Call<float>{Operation::kGenerate, matrix, matrix, tau, nullptr},
               report);
}

Status Orgqr(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<const float> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider, Call<float>{Operation::kGenerate, matrix, matrix, tau, nullptr},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryOrmqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose, DenseBlasMatrixView<const float> reflectors,
    DenseBlasVectorView<const float> tau, DenseBlasMatrixView<float> matrix,
    LapackReport& report) {
  return Query(provider,
               Call<float>{Operation::kApply, matrix, reflectors, tau, nullptr,
                           side, transpose},
               report);
}

Status Ormqr(const ReferenceLapackProvider& provider, DenseBlasSide side,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const float> reflectors,
             DenseBlasVectorView<const float> tau,
             DenseBlasMatrixView<float> matrix, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<float>{Operation::kApply, matrix, reflectors, tau,
                             nullptr, side, transpose},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> tau, LapackReport& report) {
  return Query(provider,
               Call<double>{Operation::kGeqrf, matrix, matrix, tau, tau.data()},
               report);
}

Status Geqrf(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<double> tau, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<double>{Operation::kGeqrf, matrix, matrix, tau, tau.data()}, plan,
      workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqr2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> tau, LapackReport& report) {
  return Query(provider,
               Call<double>{Operation::kGeqr2, matrix, matrix, tau, tau.data()},
               report);
}

Status Geqr2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<double> tau, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(
      provider,
      Call<double>{Operation::kGeqr2, matrix, matrix, tau, tau.data()}, plan,
      workspace, report);
}

Result<LapackWorkspacePlan> QueryOrgqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const double> tau, LapackReport& report) {
  return Query(provider,
               Call<double>{Operation::kGenerate, matrix, matrix, tau, nullptr},
               report);
}

Status Orgqr(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<const double> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Call<double>{Operation::kGenerate, matrix, matrix, tau, nullptr}, plan,
      workspace, report);
}

Result<LapackWorkspacePlan> QueryOrmqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose, DenseBlasMatrixView<const double> reflectors,
    DenseBlasVectorView<const double> tau, DenseBlasMatrixView<double> matrix,
    LapackReport& report) {
  return Query(provider,
               Call<double>{Operation::kApply, matrix, reflectors, tau, nullptr,
                            side, transpose},
               report);
}

Status Ormqr(const ReferenceLapackProvider& provider, DenseBlasSide side,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const double> reflectors,
             DenseBlasVectorView<const double> tau,
             DenseBlasMatrixView<double> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<double>{Operation::kApply, matrix, reflectors, tau,
                              nullptr, side, transpose},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqrfWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> tau, LapackReport& report) {
  return Query(provider,
               Call<std::complex<float>>{Operation::kGeqrf, matrix, matrix, tau,
                                         tau.data()},
               report);
}

Status Geqrf(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<std::complex<float>> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<float>>{Operation::kGeqrf, matrix, matrix,
                                           tau, tau.data()},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqr2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> tau, LapackReport& report) {
  return Query(provider,
               Call<std::complex<float>>{Operation::kGeqr2, matrix, matrix, tau,
                                         tau.data()},
               report);
}

Status Geqr2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<std::complex<float>> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<float>>{Operation::kGeqr2, matrix, matrix,
                                           tau, tau.data()},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryUngqrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<const std::complex<float>> tau, LapackReport& report) {
  return Query(provider,
               Call<std::complex<float>>{Operation::kGenerate, matrix, matrix,
                                         tau, nullptr},
               report);
}

Status Ungqr(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<const std::complex<float>> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<float>>{Operation::kGenerate, matrix, matrix,
                                           tau, nullptr},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryUnmqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> reflectors,
    DenseBlasVectorView<const std::complex<float>> tau,
    DenseBlasMatrixView<std::complex<float>> matrix, LapackReport& report) {
  return Query(provider,
               Call<std::complex<float>>{Operation::kApply, matrix, reflectors,
                                         tau, nullptr, side, transpose},
               report);
}

Status Unmqr(const ReferenceLapackProvider& provider, DenseBlasSide side,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const std::complex<float>> reflectors,
             DenseBlasVectorView<const std::complex<float>> tau,
             DenseBlasMatrixView<std::complex<float>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<float>>{Operation::kApply, matrix, reflectors, tau,
                                nullptr, side, transpose},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqrfWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> tau, LapackReport& report) {
  return Query(provider,
               Call<std::complex<double>>{Operation::kGeqrf, matrix, matrix,
                                          tau, tau.data()},
               report);
}

Status Geqrf(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<std::complex<double>> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<double>>{Operation::kGeqrf, matrix, matrix,
                                            tau, tau.data()},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeqr2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> tau, LapackReport& report) {
  return Query(provider,
               Call<std::complex<double>>{Operation::kGeqr2, matrix, matrix,
                                          tau, tau.data()},
               report);
}

Status Geqr2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<std::complex<double>> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<double>>{Operation::kGeqr2, matrix, matrix,
                                            tau, tau.data()},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryUngqrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<const std::complex<double>> tau, LapackReport& report) {
  return Query(provider,
               Call<std::complex<double>>{Operation::kGenerate, matrix, matrix,
                                          tau, nullptr},
               report);
}

Status Ungqr(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<const std::complex<double>> tau,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider,
                 Call<std::complex<double>>{Operation::kGenerate, matrix,
                                            matrix, tau, nullptr},
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryUnmqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> reflectors,
    DenseBlasVectorView<const std::complex<double>> tau,
    DenseBlasMatrixView<std::complex<double>> matrix, LapackReport& report) {
  return Query(provider,
               Call<std::complex<double>>{Operation::kApply, matrix, reflectors,
                                          tau, nullptr, side, transpose},
               report);
}

Status Unmqr(const ReferenceLapackProvider& provider, DenseBlasSide side,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const std::complex<double>> reflectors,
             DenseBlasVectorView<const std::complex<double>> tau,
             DenseBlasMatrixView<std::complex<double>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<double>>{Operation::kApply, matrix, reflectors, tau,
                                 nullptr, side, transpose},
      plan, workspace, report);
}

}  // namespace asc
