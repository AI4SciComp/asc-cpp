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
#include "asc/dense/providers/lapack_least_squares.h"
#include "internal_layout.h"
#include "internal_least_squares_counts.h"
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

// GELST is absent from the pinned lapack.h. These private declarations were
// checked against GNU11 -fc-prototypes-external for every exact pinned source,
// separately with ordinary and -fdefault-integer-8 compilation. The compiler
// emits int*/long*, std::complex<T>* and a trailing size_t character length.
// Keep the verified symbols and foreign types out of supported ASC headers.
extern "C" {
void LAPACK_GLOBAL_SUFFIX(sgelst, SGELST)(char* trans, lapack_int* m,
                                          lapack_int* n, lapack_int* nrhs,
                                          float* a, lapack_int* lda, float* b,
                                          lapack_int* ldb, float* work,
                                          lapack_int* lwork, lapack_int* info,
                                          std::size_t trans_length);
void LAPACK_GLOBAL_SUFFIX(dgelst, DGELST)(char* trans, lapack_int* m,
                                          lapack_int* n, lapack_int* nrhs,
                                          double* a, lapack_int* lda, double* b,
                                          lapack_int* ldb, double* work,
                                          lapack_int* lwork, lapack_int* info,
                                          std::size_t trans_length);
void LAPACK_GLOBAL_SUFFIX(cgelst,
                          CGELST)(char* trans, lapack_int* m, lapack_int* n,
                                  lapack_int* nrhs, lapack_complex_float* a,
                                  lapack_int* lda, lapack_complex_float* b,
                                  lapack_int* ldb, lapack_complex_float* work,
                                  lapack_int* lwork, lapack_int* info,
                                  std::size_t trans_length);
void LAPACK_GLOBAL_SUFFIX(zgelst,
                          ZGELST)(char* trans, lapack_int* m, lapack_int* n,
                                  lapack_int* nrhs, lapack_complex_double* a,
                                  lapack_int* lda, lapack_complex_double* b,
                                  lapack_int* ldb, lapack_complex_double* work,
                                  lapack_int* lwork, lapack_int* info,
                                  std::size_t trans_length);
}

namespace asc {
namespace {
static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
using internal_lapack_least_squares::Operation;
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
  static constexpr std::array<std::string_view, 3> kNames{"sgels", "sgelst",
                                                          "sgetsls"};
  static lapack_int Invoke(Operation operation, char transpose, lapack_int m,
                           lapack_int n, lapack_int nrhs, float* a,
                           lapack_int lda, float* b, lapack_int ldb,
                           float* work, lapack_int lwork) {
    lapack_int info = 0;
    switch (operation) {
      case Operation::kGels:
        LAPACK_sgels(&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork,
                     &info);
        break;
      case Operation::kGelst:
        LAPACK_GLOBAL_SUFFIX(sgelst, SGELST)
        (&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork, &info, 1);
        break;
      case Operation::kGetsls:
        LAPACK_sgetsls(&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work,
                       &lwork, &info);
        break;
    }
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF64;
  static constexpr std::array<std::string_view, 3> kNames{"dgels", "dgelst",
                                                          "dgetsls"};
  static lapack_int Invoke(Operation operation, char transpose, lapack_int m,
                           lapack_int n, lapack_int nrhs, double* a,
                           lapack_int lda, double* b, lapack_int ldb,
                           double* work, lapack_int lwork) {
    lapack_int info = 0;
    switch (operation) {
      case Operation::kGels:
        LAPACK_dgels(&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork,
                     &info);
        break;
      case Operation::kGelst:
        LAPACK_GLOBAL_SUFFIX(dgelst, DGELST)
        (&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork, &info, 1);
        break;
      case Operation::kGetsls:
        LAPACK_dgetsls(&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work,
                       &lwork, &info);
        break;
    }
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC64;
  static constexpr std::array<std::string_view, 3> kNames{"cgels", "cgelst",
                                                          "cgetsls"};
  static lapack_int Invoke(Operation operation, char transpose, lapack_int m,
                           lapack_int n, lapack_int nrhs,
                           std::complex<float>* a, lapack_int lda,
                           std::complex<float>* b, lapack_int ldb,
                           std::complex<float>* work, lapack_int lwork) {
    lapack_int info = 0;
    switch (operation) {
      case Operation::kGels:
        LAPACK_cgels(&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork,
                     &info);
        break;
      case Operation::kGelst:
        LAPACK_GLOBAL_SUFFIX(cgelst, CGELST)
        (&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork, &info, 1);
        break;
      case Operation::kGetsls:
        LAPACK_cgetsls(&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work,
                       &lwork, &info);
        break;
    }
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC128;
  static constexpr std::array<std::string_view, 3> kNames{"zgels", "zgelst",
                                                          "zgetsls"};
  static lapack_int Invoke(Operation operation, char transpose, lapack_int m,
                           lapack_int n, lapack_int nrhs,
                           std::complex<double>* a, lapack_int lda,
                           std::complex<double>* b, lapack_int ldb,
                           std::complex<double>* work, lapack_int lwork) {
    lapack_int info = 0;
    switch (operation) {
      case Operation::kGels:
        LAPACK_zgels(&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork,
                     &info);
        break;
      case Operation::kGelst:
        LAPACK_GLOBAL_SUFFIX(zgelst, ZGELST)
        (&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork, &info, 1);
        break;
      case Operation::kGetsls:
        LAPACK_zgetsls(&transpose, &m, &n, &nrhs, a, &lda, b, &ldb, work,
                       &lwork, &info);
        break;
    }
    return info;
  }
};

template <typename T>
struct Call {
  Operation operation;
  DenseBlasTranspose transpose;
  DenseBlasMatrixView<T> matrix;
  DenseBlasMatrixView<T> rhs;
};

template <typename T>
std::array<ConstMemoryView, 2> Operands(const Call<T>& call) {
  return {call.matrix.reachable_storage(), call.rhs.reachable_storage()};
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
extent_t RhsLeadingDimension(const Call<T>& call) {
  return std::max<extent_t>(1, call.rhs.rows());
}

template <typename T>
Status ValidateOperands(const ReferenceLapackProvider& provider,
                        const Call<T>& call) {
  constexpr auto kAdjoint = DenseBlasComplex<T>
                                ? DenseBlasTranspose::kConjugateTranspose
                                : DenseBlasTranspose::kTranspose;
  if (call.transpose != DenseBlasTranspose::kNone &&
      call.transpose != kAdjoint) {
    return Status(ErrorCode::kInvalidArgument);
  }
  for (const auto space :
       {call.matrix.memory_space(), call.rhs.memory_space()}) {
    if (!provider.context().CanAccess(space)) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  if (call.rhs.rows() != std::max(call.matrix.rows(), call.matrix.columns())) {
    return Status(ErrorCode::kShape);
  }
  for (const auto dimension :
       {call.matrix.rows(), call.matrix.columns(), call.rhs.columns(),
        internal_lapack_layout::LeadingDimension(call.matrix),
        RhsLeadingDimension(call)}) {
    if (dimension > kIntegerLimit) {
      return Status(ErrorCode::kOverflow);
    }
  }
  return CheckDisjoint(Operands(call));
}

template <typename T>
Result<LapackWorkspacePlan> Plan(const ReferenceLapackProvider& provider,
                                 const Call<T>& call) {
  Status valid = ValidateOperands(provider, call);
  if (!valid.ok()) {
    return valid;
  }
  const auto counts =
      internal_lapack_least_squares::QueryCounts<DenseBlasRealType<T>>(
          call.operation, call.matrix.rows(), call.matrix.columns(),
          call.rhs.columns(),
          internal_lapack_layout::LeadingDimension(call.matrix),
          DenseBlasComplex<T>, kIntegerLimit);
  if (!counts.ok()) {
    return counts.status();
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kNames[static_cast<std::size_t>(call.operation)],
      Native<T>::kKind,
      std::array{call.matrix.rows(), call.matrix.columns(), call.rhs.columns(),
                 internal_lapack_layout::LeadingDimension(call.matrix),
                 RhsLeadingDimension(call)},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(call.transpose),
          static_cast<std::int64_t>(call.matrix.layout()),
          static_cast<std::int64_t>(call.rhs.layout()),
          call.matrix.leading_dimension(), call.rhs.leading_dimension(),
          counts->minimum, counts->preferred},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  plan.regions[kScalar] = {counts->minimum, counts->preferred, sizeof(T),
                           alignof(T)};
  extent_t packing = 0;
  if (call.matrix.layout() == DenseBlasLayout::kRowMajor) {
    const auto count = internal_lapack_least_squares::AppendPacking(
        0, call.matrix.rows(), call.matrix.columns(), sizeof(T));
    if (!count.ok()) {
      return count.status();
    }
    packing = *count;
  }
  const auto total = internal_lapack_least_squares::AppendPacking(
      packing, call.rhs.rows(), call.rhs.columns(), sizeof(T));
  if (!total.ok()) {
    return total.status();
  }
  plan.regions[kPacking] = {*total, *total, sizeof(T), alignof(T)};
  Status bytes = internal_lapack_layout::CheckTotal(plan);
  if (!bytes.ok()) {
    return bytes;
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

Status InterpretInfo(lapack_int info, extent_t k, bool query,
                     LapackReport& report) {
  report.native_info = info;
  if (info == 0) {
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = query ? LapackOutputValidity::kUnchanged
                                   : LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  if (info > 0 && info <= k && !query) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<extent_t>(info) - 1;
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
lapack_int Invoke(const Call<T>& call, T* matrix, T* rhs, T* work,
                  lapack_int lwork) {
  T dummy_a{};
  T dummy_b{};
  const char adjoint = DenseBlasComplex<T> ? 'C' : 'T';
  const char transpose =
      call.transpose == DenseBlasTranspose::kNone ? 'N' : adjoint;
  return Native<T>::Invoke(
      call.operation, transpose, static_cast<lapack_int>(call.matrix.rows()),
      static_cast<lapack_int>(call.matrix.columns()),
      static_cast<lapack_int>(call.rhs.columns()),
      matrix == nullptr ? &dummy_a : matrix,
      static_cast<lapack_int>(
          internal_lapack_layout::LeadingDimension(call.matrix)),
      rhs == nullptr ? &dummy_b : rhs,
      static_cast<lapack_int>(RhsLeadingDimension(call)), work, lwork);
}

template <typename T>
Status QueryOne(const ReferenceLapackProvider& provider, const Call<T>& call,
                lapack_int selector, extent_t expected, LapackReport& report) {
  T value{};
  report.called_provider = true;
  const auto info =
      Invoke(call, call.matrix.data(), call.rhs.data(), &value, selector);
  Status status = InterpretInfo(
      info, std::min(call.matrix.rows(), call.matrix.columns()), true, report);
  if (!status.ok()) {
    return status;
  }
  const auto checked =
      CheckedLapackQueryEntries(static_cast<double>(std::real(value)),
                                provider.identity().integer_abi, sizeof(T));
  if (!checked.ok() || std::imag(value) != 0 ||
      std::real(value) != static_cast<DenseBlasRealType<T>>(expected)) {
    report.outcome = LapackOutcome::kPartialResult;
    return checked.ok() ? Status(ErrorCode::kProvider) : checked.status();
  }
  return Status::Ok();
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Call<T>& call, LapackReport& report) {
  Status metadata =
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
  if (call.operation == Operation::kGetsls) {
    Status status = QueryOne(provider, call, -2,
                             plan->regions[kScalar].minimum_entries, report);
    if (!status.ok()) {
      return status;
    }
  }
  Status status = QueryOne(provider, call, -1,
                           plan->regions[kScalar].preferred_entries, report);
  if (!status.ok()) {
    return status;
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
extent_t InputRows(const Call<T>& call) {
  return call.transpose == DenseBlasTranspose::kNone ? call.matrix.rows()
                                                     : call.matrix.columns();
}

template <typename T>
extent_t SolutionRows(const Call<T>& call) {
  return call.transpose == DenseBlasTranspose::kNone ? call.matrix.columns()
                                                     : call.matrix.rows();
}

template <typename T>
void PublishRhs(const T* packed, const Call<T>& call, extent_t rows) {
  if (rows == 0) {
    return;
  }
  for (extent_t j = 0; j < call.rhs.columns(); ++j) {
    for (extent_t i = 0; i < rows; ++i) {
      Entry(call.rhs, i, j) = packed[j * call.rhs.rows() + i];
    }
  }
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
  const extent_t k = std::min(call.matrix.rows(), call.matrix.columns());
  const extent_t output_rows = call.operation == Operation::kGetsls
                                   ? SolutionRows(call)
                                   : call.rhs.rows();
  if (k == 0 || call.rhs.columns() == 0) {
    if (output_rows != 0) {
      for (extent_t j = 0; j < call.rhs.columns(); ++j) {
        for (extent_t i = 0; i < output_rows; ++i) {
          Entry(call.rhs, i, j) = T{};
        }
      }
    }
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  auto* cursor = static_cast<T*>(workspace.regions[kPacking].data());
  T* matrix = internal_lapack_layout::Pack(call.matrix, cursor);
  T* rhs = cursor;
  const extent_t input_rows = InputRows(call);
  for (extent_t j = 0; j < call.rhs.columns(); ++j) {
    for (extent_t i = 0; i < input_rows; ++i) {
      rhs[j * call.rhs.rows() + i] = Entry(call.rhs, i, j);
    }
  }
  const auto lwork = static_cast<lapack_int>(std::min(
      workspace.regions[kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[kScalar].preferred_entries)));
  report.called_provider = true;
  status = InterpretInfo(
      Invoke(call, matrix, rhs,
             static_cast<T*>(workspace.regions[kScalar].data()), lwork),
      k, false, report);
  if (status.ok() || report.outcome == LapackOutcome::kSingular) {
    internal_lapack_layout::Unpack(matrix, call.matrix);
    PublishRhs(rhs, call, status.ok() ? output_rows : input_rows);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryGelsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
    LapackReport& report) {
  return Query(provider, Call<float>{Operation::kGels, transpose, matrix, rhs},
               report);
}

Status Gels(const ReferenceLapackProvider& provider,
            DenseBlasTranspose transpose, DenseBlasMatrixView<float> matrix,
            DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<float>{Operation::kGels, transpose, matrix, rhs}, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGelsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
    LapackReport& report) {
  return Query(provider, Call<double>{Operation::kGels, transpose, matrix, rhs},
               report);
}

Status Gels(const ReferenceLapackProvider& provider,
            DenseBlasTranspose transpose, DenseBlasMatrixView<double> matrix,
            DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<double>{Operation::kGels, transpose, matrix, rhs}, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGelsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs, LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<float>>{Operation::kGels, transpose, matrix, rhs},
      report);
}

Status Gels(const ReferenceLapackProvider& provider,
            DenseBlasTranspose transpose,
            DenseBlasMatrixView<std::complex<float>> matrix,
            DenseBlasMatrixView<std::complex<float>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<float>>{Operation::kGels, transpose, matrix, rhs}, plan,
      workspace, report);
}

Result<LapackWorkspacePlan> QueryGelsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs, LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<double>>{Operation::kGels, transpose, matrix, rhs},
      report);
}

Status Gels(const ReferenceLapackProvider& provider,
            DenseBlasTranspose transpose,
            DenseBlasMatrixView<std::complex<double>> matrix,
            DenseBlasMatrixView<std::complex<double>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<double>>{Operation::kGels, transpose, matrix, rhs},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGelstWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
    LapackReport& report) {
  return Query(provider, Call<float>{Operation::kGelst, transpose, matrix, rhs},
               report);
}

Status Gelst(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose, DenseBlasMatrixView<float> matrix,
             DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<float>{Operation::kGelst, transpose, matrix, rhs}, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGelstWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
    LapackReport& report) {
  return Query(provider,
               Call<double>{Operation::kGelst, transpose, matrix, rhs}, report);
}

Status Gelst(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose, DenseBlasMatrixView<double> matrix,
             DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<double>{Operation::kGelst, transpose, matrix, rhs}, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGelstWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs, LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<float>>{Operation::kGelst, transpose, matrix, rhs},
      report);
}

Status Gelst(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<float>>{Operation::kGelst, transpose, matrix, rhs},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGelstWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs, LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<double>>{Operation::kGelst, transpose, matrix, rhs},
      report);
}

Status Gelst(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasMatrixView<std::complex<double>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<double>>{Operation::kGelst, transpose, matrix, rhs},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGetslsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
    LapackReport& report) {
  return Query(provider,
               Call<float>{Operation::kGetsls, transpose, matrix, rhs}, report);
}

Status Getsls(const ReferenceLapackProvider& provider,
              DenseBlasTranspose transpose, DenseBlasMatrixView<float> matrix,
              DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<float>{Operation::kGetsls, transpose, matrix, rhs}, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGetslsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
    LapackReport& report) {
  return Query(provider,
               Call<double>{Operation::kGetsls, transpose, matrix, rhs},
               report);
}

Status Getsls(const ReferenceLapackProvider& provider,
              DenseBlasTranspose transpose, DenseBlasMatrixView<double> matrix,
              DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<double>{Operation::kGetsls, transpose, matrix, rhs}, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGetslsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs, LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<float>>{Operation::kGetsls, transpose, matrix, rhs},
      report);
}

Status Getsls(const ReferenceLapackProvider& provider,
              DenseBlasTranspose transpose,
              DenseBlasMatrixView<std::complex<float>> matrix,
              DenseBlasMatrixView<std::complex<float>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<float>>{Operation::kGetsls, transpose, matrix, rhs},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGetslsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs, LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<double>>{Operation::kGetsls, transpose, matrix, rhs},
      report);
}

Status Getsls(const ReferenceLapackProvider& provider,
              DenseBlasTranspose transpose,
              DenseBlasMatrixView<std::complex<double>> matrix,
              DenseBlasMatrixView<std::complex<double>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<double>>{Operation::kGetsls, transpose, matrix, rhs},
      plan, workspace, report);
}

}  // namespace asc
