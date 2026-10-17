#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_driver.h"
#include "internal_positive_tridiagonal_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return "sptsv";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dptsv";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "cptsv";
  } else {
    return "zptsv";
  }
}

template <typename T>
struct Operands {
  LapackPositiveDefiniteTridiagonalView<T> matrix;
  DenseBlasMatrixView<T> rhs;
  [[nodiscard]] auto Spans() const {
    return std::array<ConstMemoryView, 3>{matrix.diagonal().reachable_storage(),
                                          matrix.lower().reachable_storage(),
                                          rhs.reachable_storage()};
  }
};

template <typename T>
Status Metadata(const ReferenceLapackProvider& provider, const Operands<T>& a) {
  const auto n = a.matrix.order();
  if (a.rhs.rows() != n) {
    return Status(ErrorCode::kShape);
  }
  for (const auto& status : std::array{
           checked::Vector(provider, a.matrix.diagonal(), n),
           checked::Vector(provider, a.matrix.lower(), n == 0 ? 0 : n - 1),
           checked::Access(provider, a.rhs.reachable_storage()),
           checked::Disjoint(a.Spans())}) {
    if (!status.ok()) {
      return status;
    }
  }
  // PTSV always factors, even with zero RHS. Its active solve uses the
  // staged leading dimension N, not the caller's physical leading metadata.
  return internal_positive_tridiagonal_counts::Solve(
      n, a.rhs.columns(), n == 0 ? 1 : n, checked::kLimit);
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  const Operands<T>& a) {
  const auto status = Metadata(provider, a);
  if (!status.ok()) {
    return status;
  }
  const auto n = a.matrix.order();
  const auto nrhs = a.rhs.columns();
  const auto identity = LapackPlanIdentity::Create(
      Routine<T>(), checked::Kind<T>(),
      std::array{n, nrhs, a.rhs.leading_dimension()},
      std::array<std::int64_t, 1>{static_cast<std::int64_t>(a.rhs.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (n == 0 || nrhs == 0) {
    return plan;
  }
  if (nrhs > std::numeric_limits<extent_t>::max() / n) {
    return Status(ErrorCode::kOverflow);
  }
  const auto entries = n * nrhs;
  if (static_cast<std::uint64_t>(entries) >
      std::numeric_limits<std::size_t>::max() / sizeof(T)) {
    return Status(ErrorCode::kOverflow);
  }
  plan.regions[checked::kLayout] = {entries, entries, sizeof(T), alignof(T)};
  return plan;
}

template <typename T>
extent_t Offset(DenseBlasMatrixView<T> matrix, extent_t i, extent_t j) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? j * matrix.leading_dimension() + i
             : i * matrix.leading_dimension() + j;
}

template <typename T>
bool Finite(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
  } else {
    return std::isfinite(value);
  }
}

template <typename T>
lapack_int Native(const Operands<T>& a, T* rhs) {
  const auto n = static_cast<lapack_int>(a.matrix.order());
  const auto nrhs = static_cast<lapack_int>(a.rhs.columns());
  auto* d = a.matrix.diagonal().data();
  T unused{};
  auto* e = n < 2 ? &unused : a.matrix.lower().data();
  auto* b = nrhs == 0 ? &unused : rhs;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sptsv(&n, &nrhs, d, e, b, &n, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dptsv(&n, &nrhs, d, e, b, &n, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cptsv(&n, &nrhs, d, e, b, &n, &info);
  } else {
    LAPACK_zptsv(&n, &nrhs, d, e, b, &n, &info);
  }
  return info;
}

template <typename T>
Status Publish(const Operands<T>& a, const T* solution, LapackReport& report) {
  bool finite = true;
  const auto n = a.matrix.order();
  for (extent_t i = 0; i < n; ++i) {
    finite = finite && std::isfinite(a.matrix.diagonal().data()[i]);
    if (i + 1 < n) {
      finite = finite && Finite(a.matrix.lower().data()[i]);
    }
  }
  for (extent_t j = 0; j < a.rhs.columns(); ++j) {
    for (extent_t i = 0; i < n; ++i) {
      const auto value = solution[j * n + i];
      finite = finite && Finite(value);
      a.rhs.data()[Offset(a.rhs, i, j)] = value;
    }
  }
  if (!finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider, const Operands<T>& a,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto spans = a.Spans();
  auto status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<T>(), report);
  const auto expected = Query(provider, a);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  const auto n = a.matrix.order();
  if (n == 0) {
    return checked::Complete(report);
  }
  auto* solution = static_cast<T*>(workspace.regions[checked::kLayout].data());
  for (extent_t j = 0; j < a.rhs.columns(); ++j) {
    for (extent_t i = 0; i < n; ++i) {
      solution[j * n + i] = a.rhs.data()[Offset(a.rhs, i, j)];
    }
  }
  report.called_provider = true;
  const auto info = Native(a, solution);
  report.native_info = info;
  if (info < 0 || info > n) {
    return checked::ProviderDefect(info, report);
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  return Publish(a, solution, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<float> matrix,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, Operands<float>{matrix, rhs});
}
Status Ptsv(const ReferenceLapackProvider& provider,
            LapackPositiveDefiniteTridiagonalView<float> matrix,
            DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, Operands<float>{matrix, rhs}, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryPtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<double> matrix,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, Operands<double>{matrix, rhs});
}
Status Ptsv(const ReferenceLapackProvider& provider,
            LapackPositiveDefiniteTridiagonalView<double> matrix,
            DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, Operands<double>{matrix, rhs}, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryPtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, Operands<std::complex<float>>{matrix, rhs});
}
Status Ptsv(const ReferenceLapackProvider& provider,
            LapackPositiveDefiniteTridiagonalView<std::complex<float>> matrix,
            DenseBlasMatrixView<std::complex<float>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, Operands<std::complex<float>>{matrix, rhs}, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryPtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, Operands<std::complex<double>>{matrix, rhs});
}
Status Ptsv(const ReferenceLapackProvider& provider,
            LapackPositiveDefiniteTridiagonalView<std::complex<double>> matrix,
            DenseBlasMatrixView<std::complex<double>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, Operands<std::complex<double>>{matrix, rhs}, plan,
                 workspace, report);
}
}  // namespace asc
