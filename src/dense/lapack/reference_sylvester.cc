#include <algorithm>
#include <array>
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
#include "asc/dense/providers/lapack_sylvester.h"
#include "internal_layout.h"
#include "internal_sylvester_counts.h"
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
constexpr std::size_t kPacking =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);
constexpr extent_t kIntegerLimit = std::numeric_limits<lapack_int>::max();

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "strsyl";
  static lapack_int Invoke(char operation_a, char operation_b, lapack_int sign,
                           lapack_int m, lapack_int n, const float* a,
                           const float* b, float* c, lapack_int ldc,
                           float& scale) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_strsyl(&operation_a, &operation_b, &sign, &m, &n, a, &m, b, &n, c,
                  &ldc, &scale, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dtrsyl";
  static lapack_int Invoke(char operation_a, char operation_b, lapack_int sign,
                           lapack_int m, lapack_int n, const double* a,
                           const double* b, double* c, lapack_int ldc,
                           double& scale) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dtrsyl(&operation_a, &operation_b, &sign, &m, &n, a, &m, b, &n, c,
                  &ldc, &scale, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "ctrsyl";
  static lapack_int Invoke(char operation_a, char operation_b, lapack_int sign,
                           lapack_int m, lapack_int n,
                           const std::complex<float>* a,
                           const std::complex<float>* b, std::complex<float>* c,
                           lapack_int ldc, float& scale) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ctrsyl(&operation_a, &operation_b, &sign, &m, &n, a, &m, b, &n, c,
                  &ldc, &scale, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr LapackScalarKind kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "ztrsyl";
  static lapack_int Invoke(char operation_a, char operation_b, lapack_int sign,
                           lapack_int m, lapack_int n,
                           const std::complex<double>* a,
                           const std::complex<double>* b,
                           std::complex<double>* c, lapack_int ldc,
                           double& scale) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ztrsyl(&operation_a, &operation_b, &sign, &m, &n, a, &m, b, &n, c,
                  &ldc, &scale, &info);
    return info;
  }
};

template <typename T>
struct Call {
  DenseBlasTranspose operation_a;
  DenseBlasTranspose operation_b;
  LapackSylvesterSign sign;
  DenseBlasMatrixView<const T> a;
  DenseBlasMatrixView<const T> b;
  DenseBlasMatrixView<T> c;
};

template <typename T>
ConstMemoryView ObjectStorage(const T& object) {
  return {&object, sizeof(object), MemorySpace::kHost};
}

template <typename T>
std::array<ConstMemoryView, 3> Operands(const Call<T>& call) {
  return {call.a.reachable_storage(), call.b.reachable_storage(),
          call.c.reachable_storage()};
}

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  const auto a = reinterpret_cast<std::uintptr_t>(first.data());
  const auto b = reinterpret_cast<std::uintptr_t>(second.data());
  return first.size() != 0 && second.size() != 0 &&
         (a <= b ? b - a < first.size() : a - b < second.size());
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
  Status status = CheckDisjoint(metadata);
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
void StartReport(const ReferenceLapackProvider& provider,
                 LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  std::copy(Native<T>::kName.begin(), Native<T>::kName.end(),
            report.routine.begin());
}

template <typename T>
bool ValidOperation(DenseBlasTranspose operation) {
  return operation == DenseBlasTranspose::kNone ||
         operation == DenseBlasTranspose::kConjugateTranspose ||
         (!DenseBlasComplex<T> && operation == DenseBlasTranspose::kTranspose);
}

char OperationCharacter(DenseBlasTranspose operation) {
  switch (operation) {
    case DenseBlasTranspose::kNone:
      return 'N';
    case DenseBlasTranspose::kTranspose:
      return 'T';
    case DenseBlasTranspose::kConjugateTranspose:
      return 'C';
  }
  return '?';
}

template <typename T>
extent_t LeadingDimension(const Call<T>& call) {
  // A single-column C never addresses its ASC column stride; normalizing it
  // avoids narrowing an unused stride. The original stride remains in the key.
  if (call.c.rows() == 0 || call.c.columns() == 0) {
    return 1;
  }
  if (call.c.layout() == DenseBlasLayout::kRowMajor || call.c.columns() == 1) {
    return call.c.rows();
  }
  return call.c.leading_dimension();
}

template <typename T>
Result<LapackWorkspacePlan> Plan(const ReferenceLapackProvider& provider,
                                 const Call<T>& call) {
  if (!ValidOperation<T>(call.operation_a) ||
      !ValidOperation<T>(call.operation_b) ||
      (call.sign != LapackSylvesterSign::kPlus &&
       call.sign != LapackSylvesterSign::kMinus)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (call.a.rows() != call.a.columns() || call.b.rows() != call.b.columns() ||
      call.c.rows() != call.a.rows() || call.c.columns() != call.b.rows()) {
    return Status(ErrorCode::kShape);
  }
  for (const auto operand : Operands(call)) {
    if (!provider.context().CanAccess(operand.space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  Status status = CheckDisjoint(Operands(call));
  if (!status.ok()) {
    return status;
  }
  status = internal_lapack_sylvester::CheckDimensions(
      call.a.rows(), call.b.rows(), LeadingDimension(call),
      call.operation_a != DenseBlasTranspose::kNone,
      call.operation_b != DenseBlasTranspose::kNone, kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  const auto packing = internal_lapack_sylvester::PackingEntries(
      call.a.rows(), call.b.rows(),
      call.c.layout() == DenseBlasLayout::kRowMajor, sizeof(T));
  if (!packing.ok()) {
    return packing.status();
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kKind,
      std::array{call.a.rows(), call.b.rows(), LeadingDimension(call)},
      std::array<std::int64_t, 9>{static_cast<std::int64_t>(call.operation_a),
                                  static_cast<std::int64_t>(call.operation_b),
                                  static_cast<std::int64_t>(call.sign),
                                  static_cast<std::int64_t>(call.a.layout()),
                                  static_cast<std::int64_t>(call.b.layout()),
                                  static_cast<std::int64_t>(call.c.layout()),
                                  call.a.leading_dimension(),
                                  call.b.leading_dimension(),
                                  call.c.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan result{*key};
  result.regions[kPacking] = {*packing, *packing, sizeof(T), alignof(T)};
  status = internal_lapack_layout::CheckTotal(result);
  if (!status.ok()) {
    return status;
  }
  return result;
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
  const auto offset = matrix.layout() == DenseBlasLayout::kColumnMajor
                          ? column * matrix.leading_dimension() + row
                          : row * matrix.leading_dimension() + column;
  return matrix.data()[offset];
}

template <typename T>
Status CheckSchur(DenseBlasMatrixView<const T> matrix) {
  if constexpr (!DenseBlasComplex<T>) {
    bool previous_block = false;
    for (extent_t i = 0; i + 1 < matrix.rows(); ++i) {
      const T lower = Entry(matrix, i + 1, i);
      if (lower == 0) {
        previous_block = false;
        continue;
      }
      const T upper = Entry(matrix, i, i + 1);
      const T first = Entry(matrix, i, i);
      const T second = Entry(matrix, i + 1, i + 1);
      if (previous_block || !std::isfinite(lower) || !std::isfinite(upper) ||
          !std::isfinite(first) || first != second || upper == 0 ||
          std::signbit(lower) == std::signbit(upper)) {
        return Status(ErrorCode::kInvalidArgument);
      }
      previous_block = true;
    }
  }
  return Status::Ok();
}

template <typename T>
T* PackSchur(DenseBlasMatrixView<const T> matrix, T*& cursor) {
  T* result = cursor;
  const extent_t n = matrix.rows();
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < n; ++i) {
      const bool meaningful = i <= j || (!DenseBlasComplex<T> && i == j + 1);
      result[j * n + i] = meaningful ? Entry(matrix, i, j) : T{};
    }
  }
  cursor += n * n;
  return result;
}

template <typename Real>
bool FiniteSumOverflows(Real first, Real second) {
  // Bound the mathematical sum without evaluating the overflowing addition.
  // Nonfinite input retains the separate existing postflight warning policy.
  if (!std::isfinite(first) || !std::isfinite(second)) {
    return false;
  }
  const Real limit = std::numeric_limits<Real>::max();
  return (second > 0 && first > limit - second) ||
         (second < 0 && first < -limit - second);
}

template <typename T>
Status CheckDiagonalSums(const Call<T>& call) {
  using Real = DenseBlasRealType<T>;
  const Real sign = call.sign == LapackSylvesterSign::kPlus ? 1 : -1;
  const Real imaginary_a =
      call.operation_a == DenseBlasTranspose::kConjugateTranspose ? -1 : 1;
  const Real imaginary_b =
      call.operation_b == DenseBlasTranspose::kConjugateTranspose ? -sign
                                                                  : sign;
  for (extent_t j = 0; j < call.b.rows(); ++j) {
    const T b = Entry(call.b, j, j);
    for (extent_t i = 0; i < call.a.rows(); ++i) {
      const T a = Entry(call.a, i, i);
      // Pinned TRSYL forms these coefficients before its guarded division.
      // Finite overflow can otherwise return a wrong finite X with INFO=0.
      if (FiniteSumOverflows(std::real(a), sign * std::real(b)) ||
          FiniteSumOverflows(imaginary_a * std::imag(a),
                             imaginary_b * std::imag(b))) {
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  return Status::Ok();
}

template <typename T>
bool FiniteResult(const T* c, extent_t m, extent_t n, extent_t ldc) {
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < m; ++i) {
      const T value = c[j * ldc + i];
      if (!std::isfinite(std::real(value)) ||
          !std::isfinite(std::imag(value))) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Call<T>& call, LapackReport& report) {
  Status status =
      CheckMetadata(std::array{ObjectStorage(provider), ObjectStorage(report)},
                    Operands(call), nullptr);
  if (!status.ok()) {
    return status;
  }
  StartReport<T>(provider, report);
  const auto result = Plan(provider, call);
  if (result.ok()) {
    report.outcome = LapackOutcome::kSuccess;
  }
  return result;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider, const Call<T>& call,
               DenseBlasRealType<T>& scale, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{call.a.reachable_storage(),
                            call.b.reachable_storage(),
                            call.c.reachable_storage(), ObjectStorage(scale)};
  Status status =
      CheckMetadata(std::array{ObjectStorage(provider), ObjectStorage(plan),
                               ObjectStorage(workspace), ObjectStorage(report)},
                    operands, &workspace);
  if (!status.ok()) {
    return status;
  }
  StartReport<T>(provider, report);
  status = CheckDisjoint(operands);
  if (!status.ok()) {
    return status;
  }
  const auto expected = Plan(provider, call);
  if (!expected.ok()) {
    return expected.status();
  }
  status = ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const extent_t m = call.a.rows();
  const extent_t n = call.b.rows();
  if (m == 0 || n == 0) {
    scale = 1;
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  status = CheckSchur(call.a);
  if (!status.ok()) {
    return status;
  }
  status = CheckSchur(call.b);
  if (!status.ok()) {
    return status;
  }
  status = CheckDiagonalSums(call);
  if (!status.ok()) {
    return status;
  }
  auto* cursor = static_cast<T*>(workspace.regions[kPacking].data());
  const T* a = PackSchur(call.a, cursor);
  const T* b = PackSchur(call.b, cursor);
  T* c = internal_lapack_layout::Pack(call.c, cursor);
  DenseBlasRealType<T> native_scale = 1;
  report.called_provider = true;
  const lapack_int info = Native<T>::Invoke(
      OperationCharacter(call.operation_a),
      OperationCharacter(call.operation_b), static_cast<lapack_int>(call.sign),
      static_cast<lapack_int>(m), static_cast<lapack_int>(n), a, b, c,
      static_cast<lapack_int>(LeadingDimension(call)), native_scale);
  report.native_info = info;
  if (info < 0 || info > 1 || !std::isfinite(native_scale) ||
      native_scale < 0 || native_scale > 1) {
    report.outcome = info < 0 ? LapackOutcome::kProviderArgument
                              : LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    if (info < 0 && info != std::numeric_limits<lapack_int>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  const bool warning = info == 1 || native_scale == 0 ||
                       !FiniteResult(c, m, n, LeadingDimension(call));
  internal_lapack_layout::Unpack(c, call.c);
  scale = native_scale;
  report.outcome =
      warning ? LapackOutcome::kAccuracyWarning : LapackOutcome::kSuccess;
  report.output_validity = warning ? LapackOutputValidity::kDocumentedPartial
                                   : LapackOutputValidity::kComplete;
  return warning ? Status(ErrorCode::kNumerical) : Status::Ok();
}
}  // namespace

Result<LapackWorkspacePlan> QueryTrsylWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const float> a, DenseBlasMatrixView<const float> b,
    DenseBlasMatrixView<float> c, LapackReport& report) {
  return Query(provider, Call<float>{operation_a, operation_b, sign, a, b, c},
               report);
}

Status Trsyl(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation_a, DenseBlasTranspose operation_b,
             LapackSylvesterSign sign, DenseBlasMatrixView<const float> a,
             DenseBlasMatrixView<const float> b, DenseBlasMatrixView<float> c,
             float& scale, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, Call<float>{operation_a, operation_b, sign, a, b, c},
                 scale, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTrsylWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const double> a, DenseBlasMatrixView<const double> b,
    DenseBlasMatrixView<double> c, LapackReport& report) {
  return Query(provider, Call<double>{operation_a, operation_b, sign, a, b, c},
               report);
}

Status Trsyl(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation_a, DenseBlasTranspose operation_b,
             LapackSylvesterSign sign, DenseBlasMatrixView<const double> a,
             DenseBlasMatrixView<const double> b, DenseBlasMatrixView<double> c,
             double& scale, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider,
                 Call<double>{operation_a, operation_b, sign, a, b, c}, scale,
                 plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTrsylWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<const std::complex<float>> b,
    DenseBlasMatrixView<std::complex<float>> c, LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<float>>{operation_a, operation_b, sign, a, b, c},
      report);
}

Status Trsyl(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation_a, DenseBlasTranspose operation_b,
             LapackSylvesterSign sign,
             DenseBlasMatrixView<const std::complex<float>> a,
             DenseBlasMatrixView<const std::complex<float>> b,
             DenseBlasMatrixView<std::complex<float>> c, float& scale,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<float>>{operation_a, operation_b, sign, a, b, c}, scale,
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTrsylWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<const std::complex<double>> b,
    DenseBlasMatrixView<std::complex<double>> c, LapackReport& report) {
  return Query(
      provider,
      Call<std::complex<double>>{operation_a, operation_b, sign, a, b, c},
      report);
}

Status Trsyl(const ReferenceLapackProvider& provider,
             DenseBlasTranspose operation_a, DenseBlasTranspose operation_b,
             LapackSylvesterSign sign,
             DenseBlasMatrixView<const std::complex<double>> a,
             DenseBlasMatrixView<const std::complex<double>> b,
             DenseBlasMatrixView<std::complex<double>> c, double& scale,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(
      provider,
      Call<std::complex<double>>{operation_a, operation_b, sign, a, b, c},
      scale, plan, workspace, report);
}

}  // namespace asc
