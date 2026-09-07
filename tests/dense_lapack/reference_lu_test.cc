#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "allocation_audit.h"
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
#include "fault_injection.h"

namespace {
using asc_dense_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kLu = asc::LapackFactorFamily::kLuPartialPivot;
constexpr auto kPivot =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);

template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::abort();
  }
  return std::move(*value);
}

template <typename T>
T Value(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return T{static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T, std::size_t Size>
auto Matrix(std::array<T, Size>& storage, asc::extent_t rows,
            asc::extent_t columns, asc::extent_t ld,
            asc::DenseBlasLayout layout = kColumn) {
  return Take(asc::DenseBlasMatrixView<T>::Create(
      storage.data(), rows, columns, layout, ld,
      {storage.data(), sizeof(storage), kHost}));
}
template <typename T, std::size_t Size>
auto ConstMatrix(const std::array<T, Size>& storage, asc::extent_t rows,
                 asc::extent_t columns, asc::extent_t ld) {
  return Take(asc::DenseBlasMatrixView<const T>::Create(
      storage.data(), rows, columns, kColumn, ld,
      {storage.data(), sizeof(storage), kHost}));
}
template <std::size_t Size>
auto Pivots(std::array<asc::index_t, Size>& storage, asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      storage.data(), count, 1, {storage.data(), sizeof(storage), kHost}));
}
template <std::size_t Size>
auto Raw(const std::array<asc::index_t, Size>& storage, asc::extent_t count) {
  return Take(asc::RawLapackPivotView::Create(
      storage.data(), count, kLu, {storage.data(), sizeof(storage), kHost}));
}
struct Scratch {
  alignas(std::max_align_t) std::array<std::byte, 256> bytes{};
  asc::LapackWorkspace workspace() {
    asc::LapackWorkspace result;
    result.regions[kPivot] = {bytes.data(), bytes.size(), kHost};
    return result;
  }
};

template <typename Operation>
asc::Status WithoutAllocation(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe cpp_probe;
  asc_lapack_test::BeginAllocationAudit();
  asc::Status status = operation();
  const std::size_t c_calls = asc_lapack_test::EndAllocationAudit();
  const auto cpp_calls = cpp_probe.count();
  ASC_DENSE_TEST_EQ(test, c_calls, 0U);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp_calls, 0));
  return status;
}

template <typename T, std::size_t Capacity = 64>
struct NumericalCase {
  using Real = asc::DenseBlasRealType<T>;
  int m;
  int n;
  int ld;
  Real scale;
  std::array<T, Capacity> values;
  std::array<T, Capacity> original;
  std::array<asc::index_t, Capacity> pivots;
  NumericalCase(int rows, int columns, Real magnitude)
      : m(rows), n(columns), ld(rows + 2), scale(magnitude) {
    values.fill(Value<T>(-701, 79));
    pivots.fill(-401);
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < m; ++i) {
        values[j * ld + i] =
            scale * Value<T>((i == j ? 7 : 0) + ((i * 3 + j * 2) % 5) - 2,
                             (i + 2 * j) % 3 - 1);
      }
    }
    values[0] = T{};
    original = values;
  }
  [[nodiscard]] T Op(int row, int column,
                     asc::DenseBlasTranspose transpose) const {
    T value = transpose == asc::DenseBlasTranspose::kNone
                  ? original[column * ld + row]
                  : original[row * ld + column];
    if constexpr (asc::DenseBlasComplex<T>) {
      if (transpose == asc::DenseBlasTranspose::kConjugateTranspose) {
        value = std::conj(value);
      }
    }
    return value;
  }
};

template <typename T, std::size_t Capacity>
void CheckReconstruction(TestContext& test,
                         const NumericalCase<T, Capacity>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  const auto& [m, n, ld, scale, values, original, pivots] = sample;
  auto permuted = original;
  for (int i = 0; i < std::min(m, n); ++i) {
    const int pivot = static_cast<int>(pivots[i] - 1);
    ASC_DENSE_TEST_CHECK(test, pivot >= i && pivot < m);
    if (pivot < i || pivot >= m) {
      std::abort();
    }
    for (int j = 0; j < n; ++j) {
      std::swap(permuted[j * ld + i], permuted[j * ld + pivot]);
    }
  }
  Real error = 0;
  Real norm = 0;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < m; ++i) {
      T sum{};
      for (int k = 0; k < std::min(m, n); ++k) {
        T lower{};
        if (i == k) {
          lower = T{1};
        }
        if (i > k) {
          lower = values[k * ld + i];
        }
        const T upper = k <= j ? values[j * ld + k] : T{};
        sum += lower * upper;
      }
      error = std::max(error, std::abs(sum - permuted[j * ld + i]));
      norm = std::max(norm, std::abs(original[j * ld + i]));
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, error <= 256 * std::numeric_limits<Real>::epsilon() * norm);
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i / ld >= static_cast<std::size_t>(n) ||
        i % ld >= static_cast<std::size_t>(m)) {
      ASC_DENSE_TEST_EQ(test, values[i], original[i]);
    }
  }
  ASC_DENSE_TEST_EQ(test, pivots[std::min(m, n)], -401);
  std::printf("GETRF scalar=%zu/%s shape=%dx%d scale=%g residual=%g\n",
              sizeof(T), asc::DenseBlasComplex<T> ? "complex" : "real", m, n,
              static_cast<double>(scale), static_cast<double>(error / norm));
}

template <typename T>
std::array<T, 24> AssembleRhs(const NumericalCase<T>& sample,
                              asc::DenseBlasTranspose transpose) {
  std::array<T, 24> rhs;
  rhs.fill(Value<T>(-811, 89));
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      T sum{};
      for (int k = 0; k < sample.n; ++k) {
        sum += sample.Op(i, k, transpose) * Value<T>(k + 2 * j + 1, j - k);
      }
      rhs[j * 5 + i] = sum;
    }
  }
  return rhs;
}

template <typename T>
void CheckSolution(TestContext& test, const NumericalCase<T>& sample,
                   asc::DenseBlasTranspose transpose,
                   const std::array<T, 24>& rhs,
                   const std::array<T, 24>& original_rhs) {
  using Real = asc::DenseBlasRealType<T>;
  Real error = 0;
  Real residual = 0;
  Real norm = 0;
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      error = std::max(
          error, std::abs(rhs[j * 5 + i] - Value<T>(i + 2 * j + 1, j - i)));
      T sum{};
      for (int k = 0; k < sample.n; ++k) {
        sum += sample.Op(i, k, transpose) * rhs[j * 5 + k];
      }
      residual = std::max(residual, std::abs(sum - original_rhs[j * 5 + i]));
      norm = std::max(norm, std::abs(original_rhs[j * 5 + i]));
    }
  }
  constexpr Real kTolerance = 256 * std::numeric_limits<Real>::epsilon();
  ASC_DENSE_TEST_CHECK(test, error < 20 * kTolerance);
  ASC_DENSE_TEST_CHECK(test, residual <= kTolerance * norm);
  for (std::size_t i = 0; i < rhs.size(); ++i) {
    if (i / 5 >= 2 || i % 5 >= static_cast<std::size_t>(sample.n)) {
      ASC_DENSE_TEST_EQ(test, rhs[i], original_rhs[i]);
    }
  }
  std::printf("GETRS scalar=%zu/%s mode=%d scale=%g error=%g residual=%g\n",
              sizeof(T), asc::DenseBlasComplex<T> ? "complex" : "real",
              static_cast<int>(transpose), static_cast<double>(sample.scale),
              static_cast<double>(error), static_cast<double>(residual / norm));
}

template <typename T>
void SolveCases(TestContext& test, const asc::ReferenceLapackProvider& provider,
                NumericalCase<T>& sample,
                const asc::LapackReport& factor_report) {
  const auto factor = Take(asc::LapackLuFactorView<T>::Create(
      ConstMatrix(sample.values, sample.n, sample.n, sample.ld),
      Raw(sample.pivots, sample.n), factor_report));
  const auto frozen_factor = sample.values;
  const auto frozen_pivots = sample.pivots;
  Scratch scratch;
  for (auto trans :
       {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
        asc::DenseBlasTranspose::kConjugateTranspose}) {
    auto rhs = AssembleRhs(sample, trans);
    const auto original_rhs = rhs;
    const auto rhs_view = Matrix(rhs, sample.n, 2, 5);
    const auto plan =
        Take(asc::QueryGetrsWorkspace(provider, trans, factor, rhs_view));
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Getrs(provider, trans, factor, rhs_view, plan,
                        scratch.workspace(), report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    CheckSolution(test, sample, trans, rhs, original_rhs);
    ASC_DENSE_TEST_EQ(test, sample.values, frozen_factor);
    ASC_DENSE_TEST_EQ(test, sample.pivots, frozen_pivots);
  }
}

template <typename T>
void BlockedFactor(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  // Pinned ILAENV selects NB=64 for GETRF. min(m,n)=67 exercises blocking.
  NumericalCase<T, 5000> sample(67, 67, asc::DenseBlasRealType<T>{1});
  for (int i = 1; i < 67; ++i) {
    sample.values[i * sample.ld + i] = Value<T>(400);
  }
  sample.original = sample.values;
  const auto matrix = Matrix(sample.values, 67, 67, sample.ld);
  const auto pivot = Pivots(sample.pivots, 67);
  const auto plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
  alignas(std::max_align_t) std::array<std::byte, 1024> storage{};
  asc::LapackWorkspace workspace;
  workspace.regions[kPivot] = {storage.data(), storage.size(), kHost};
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Getrf(provider, matrix, pivot, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  if (status.ok()) {
    CheckReconstruction(test, sample);
  }
}

template <typename T>
void SingularScalar(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  std::array<T, 8> values{T{1}, T{2}, T{}, T{}, T{2}, T{4}};
  std::array<asc::index_t, 2> pivots{};
  const auto matrix = Matrix(values, 2, 2, 4);
  const auto pivot = Pivots(pivots, 2);
  const auto plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
  Scratch scratch;
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Getrf(provider, matrix, pivot, plan, scratch.workspace(),
                      report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 2);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackLuFactorView<T>::Create(ConstMatrix(values, 2, 2, 4),
                                                Raw(pivots, 2), report)
                 .ok());
}

template <typename T>
void Numerics(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  // No warm-up: the first GETRF in each process is allocation-observed.
  for (const auto shape :
       {std::array{3, 3}, std::array{5, 3}, std::array{3, 5}}) {
    for (Real scale : {Real{1}, Real{0.0001}, Real{10000}}) {
      NumericalCase<T> sample(shape[0], shape[1], scale);
      const auto matrix = Matrix(sample.values, sample.m, sample.n, sample.ld);
      const auto pivot = Pivots(sample.pivots, std::min(sample.m, sample.n));
      const auto plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
      ASC_DENSE_TEST_EQ(test, plan.regions[kPivot].minimum_entries,
                        pivot.size());
      ASC_DENSE_TEST_EQ(
          test, plan.regions[kPivot].entry_bytes,
          provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64 ? 4U
                                                                          : 8U);
      Scratch scratch;
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return asc::Getrf(provider, matrix, pivot, plan, scratch.workspace(),
                          report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      if (!status.ok()) {
        continue;
      }
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
      ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
      CheckReconstruction(test, sample);
      if (sample.m == sample.n) {
        SolveCases(test, provider, sample, report);
      }
    }
  }
  SingularScalar<T>(test, provider);
  BlockedFactor<T>(test, provider);
}

void WorkspaceRejections(TestContext& test,
                         const asc::ReferenceLapackProvider& provider) {
  std::array<double, 16> a{2, 1, 0, 0, 1, 3};
  std::array<asc::index_t, 4> pivots{-9, -9, -9, -9};
  const auto matrix = Matrix(a, 2, 2, 4);
  const auto pivot = Pivots(pivots, 2);
  const auto plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
  Scratch scratch;
  asc::LapackReport report;
  auto reject = [&](const asc::LapackWorkspacePlan& candidate,
                    const asc::LapackWorkspace& workspace) {
    const auto before_a = a;
    const auto before_p = pivots;
    report.called_provider = true;
    report.native_info = 77;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Getrf(provider, matrix, pivot, candidate, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
    ASC_DENSE_TEST_EQ(test, a, before_a);
    ASC_DENSE_TEST_EQ(test, pivots, before_p);
  };
  reject(plan, {});
  auto insufficient = scratch.workspace();
  insufficient.regions[kPivot] = {scratch.bytes.data(), 1, kHost};
  reject(plan, insufficient);
  auto misaligned = scratch.workspace();
  misaligned.regions[kPivot] = {scratch.bytes.data() + 1, 128, kHost};
  reject(plan, misaligned);
  auto overlap = scratch.workspace();
  overlap.regions[kPivot] = {a.data(), sizeof(a), kHost};
  reject(plan, overlap);
  auto device = scratch.workspace();
  device.regions[kPivot] = {scratch.bytes.data(), 128,
                            asc::MemorySpace::kDevice};
  reject(plan, device);
  auto tampered = plan;
  tampered.regions[kPivot].minimum_entries = 0;
  reject(tampered, scratch.workspace());
  tampered = plan;
  tampered.regions[kPivot].entry_bytes = 1;
  reject(tampered, scratch.workspace());
  auto identity = provider.identity();
  identity.build_sha256[0] ^= std::byte{1};
  tampered = plan;
  tampered.identity = Take(asc::LapackPlanIdentity::Create(
      "dgetrf", asc::LapackScalarKind::kF64, {}, {}, identity));
  reject(tampered, scratch.workspace());
  const auto before_a = a;
  const auto before_p = pivots;
  const auto row_major = Matrix(a, 2, 2, 4, asc::DenseBlasLayout::kRowMajor);
  const auto row_plan =
      Take(asc::QueryGetrfWorkspace(provider, row_major, pivot));
  ASC_DENSE_TEST_EQ(test,
                    row_plan
                        .regions[static_cast<std::size_t>(
                            asc::LapackWorkspaceKind::kLayoutConversion)]
                        .minimum_entries,
                    4);
  ASC_DENSE_TEST_CHECK(test, !asc::Getrf(provider, row_major, pivot, row_plan,
                                         scratch.workspace(), report)
                                  .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, a, before_a);
  ASC_DENSE_TEST_EQ(test, pivots, before_p);
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::QueryGetrfWorkspace(provider, matrix, Pivots(pivots, 1)).ok());
}

void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  std::array<double, 16> a{2, 1, 0, 0, 1, 3};
  std::array<asc::index_t, 4> pivots{-9, -9, -9, -9};
  const auto matrix = Matrix(a, 2, 2, 4);
  const auto pivot = Pivots(pivots, 2);
  const auto plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
  Scratch scratch;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Getrf(provider, matrix, pivot, plan,
                                                 scratch.workspace(), report);
                             }).ok());
  const auto factor = Take(asc::LapackLuFactorView<double>::Create(
      ConstMatrix(a, 2, 2, 4), Raw(pivots, 2), report));
  std::array<double, 16> b{1, 2};
  const auto rhs = Matrix(b, 2, 1, 4);
  const auto solve_plan = Take(asc::QueryGetrsWorkspace(
      provider, asc::DenseBlasTranspose::kNone, factor, rhs));
  const auto before_b = b;
  auto native_report = report;
  native_report.provider = {};
  const auto native_factor = Take(asc::LapackLuFactorView<double>::Create(
      ConstMatrix(a, 2, 2, 4), Raw(pivots, 2), native_report));
  ASC_DENSE_TEST_CHECK(
      test, !asc::QueryGetrsWorkspace(provider, asc::DenseBlasTranspose::kNone,
                                      native_factor, rhs)
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::QueryGetrsWorkspace(provider, asc::DenseBlasTranspose::kNone,
                                      factor, matrix)
                 .ok());
  auto stale_solve = WithoutAllocation(test, [&] {
    return asc::Getrs(provider, asc::DenseBlasTranspose::kTranspose, factor,
                      rhs, solve_plan, scratch.workspace(), report);
  });
  ASC_DENSE_TEST_CHECK(test, !stale_solve.ok() && !report.called_provider);
  ASC_DENSE_TEST_EQ(test, b, before_b);
  pivots[0] = 0;
  auto status = WithoutAllocation(test, [&] {
    return asc::Getrs(provider, asc::DenseBlasTranspose::kNone, factor, rhs,
                      solve_plan, scratch.workspace(), report);
  });
  ASC_DENSE_TEST_CHECK(
      test, !status.ok() && !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, b, before_b);
  pivots[0] = 1;
  a[0] = 0;
  status = WithoutAllocation(test, [&] {
    return asc::Getrs(provider, asc::DenseBlasTranspose::kNone, factor, rhs,
                      solve_plan, scratch.workspace(), report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, b, before_b);
}

void SingularAndEmpty(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  std::array<double, 16> a{2, 1, 0, 0, 1, 3};
  std::array<asc::index_t, 4> pivots{-9, -9, -9, -9};
  const auto matrix = Matrix(a, 2, 2, 4);
  const auto pivot = Pivots(pivots, 2);
  const auto plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
  Scratch scratch;
  asc::LapackReport report;
  asc::Status status;
  a = {1, 2, 0, 0, 2, 4};
  status = WithoutAllocation(test, [&] {
    return asc::Getrf(provider, matrix, pivot, plan, scratch.workspace(),
                      report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info, 2);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackLuFactorView<double>::Create(ConstMatrix(a, 2, 2, 4),
                                                     Raw(pivots, 2), report)
                 .ok());
  const auto empty = Matrix(a, 0, 0, 1);
  const auto no_pivots = Pivots(pivots, 0);
  const auto empty_plan =
      Take(asc::QueryGetrfWorkspace(provider, empty, no_pivots));
  status = WithoutAllocation(test, [&] {
    return asc::Getrf(provider, empty, no_pivots, empty_plan, {}, report);
  });
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && !report.called_provider && !report.native_info);
  if (provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64) {
    const auto too_wide = Take(asc::DenseBlasMatrixView<double>::Create(
        nullptr, 0, 0x80000000LL, kColumn, 1, {nullptr, 0, kHost}));
    ASC_DENSE_TEST_EQ(
        test,
        asc::QueryGetrfWorkspace(provider, too_wide, no_pivots).status().code(),
        asc::ErrorCode::kOverflow);
  } else {
    const auto large_empty = Take(asc::DenseBlasMatrixView<double>::Create(
        nullptr, 0, 0x80000000LL, kColumn, 1, {nullptr, 0, kHost}));
    ASC_DENSE_TEST_CHECK(
        test, asc::QueryGetrfWorkspace(provider, large_empty, no_pivots).ok());
  }
}

void ProviderDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  std::array<double, 16> a{2, 1, 0, 0, 1, 3};
  std::array<asc::index_t, 4> pivots{-9, -9, -9, -9};
  const auto matrix = Matrix(a, 2, 2, 4);
  const auto pivot = Pivots(pivots, 2);
  const auto plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
  Scratch scratch;
  asc::LapackReport report;
  asc::Status status;
  for (auto fault : {asc_lapack_test::InjectedFault::kNegativeInfo,
                     asc_lapack_test::InjectedFault::kInvalidPivot,
                     asc_lapack_test::InjectedFault::kExcessInfo}) {
    a = {2, 1, 0, 0, 1, 3};
    pivots.fill(-71);
    const auto before_pivots = pivots;
    asc_lapack_test::SetInjectedFault(fault);
    status = WithoutAllocation(test, [&] {
      return asc::Getrf(provider, matrix, pivot, plan, scratch.workspace(),
                        report);
    });
    asc_lapack_test::SetInjectedFault(asc_lapack_test::InjectedFault::kNone);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
    if (fault == asc_lapack_test::InjectedFault::kNegativeInfo) {
      ASC_DENSE_TEST_EQ(test, report.native_info, -4);
      ASC_DENSE_TEST_EQ(test, report.native_argument, 4);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        asc::LapackOutcome::kProviderArgument);
    }
  }
  std::puts(
      "Preflight: capacity/alignment/alias/placement/staleness/tamper/pivots/"
      "zero-diagonal/singular/empty passed; allocation audit includes errors");
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar = argv[1];
  if (scalar == "s") {
    Numerics<float>(test, provider);
  } else if (scalar == "d") {
    Numerics<double>(test, provider);
  } else if (scalar == "c") {
    Numerics<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Numerics<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  WorkspaceRejections(test, provider);
  Preflight(test, provider);
  SingularAndEmpty(test, provider);
  ProviderDefects(test, provider);
  return test.Finish();
}
