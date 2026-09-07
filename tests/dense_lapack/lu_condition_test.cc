#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
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
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "lu_condition_faults.h"

namespace {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kOne = asc::LapackConditionNorm::kOne;
constexpr auto kInfinity = asc::LapackConditionNorm::kInfinity;
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "Unexpected setup failure %d\n",
                 static_cast<int>(result.status().code()));
    std::abort();
  }
  return std::move(*result);
}

template <typename Operation>
auto WithoutAllocation(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe cpp_probe;
  asc_lapack_test::BeginAllocationAudit();
  auto result = operation();
  const auto c_calls = asc_lapack_test::EndAllocationAudit();
  const auto cpp_calls = cpp_probe.count();
  ASC_DENSE_TEST_EQ(test, c_calls, 0U);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp_calls, 0));
  return result;
}

template <typename T>
T Value(long double real, long double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
Wide ToWide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}

long double Norm(const std::array<Wide, 64>& matrix, asc::extent_t n,
                 asc::LapackConditionNorm norm) {
  long double maximum = 0;
  for (asc::extent_t i = 0; i < n; ++i) {
    long double sum = 0;
    for (asc::extent_t j = 0; j < n; ++j) {
      sum += std::abs(matrix[norm == kOne ? j * n + i : i * n + j]);
    }
    maximum = std::max(maximum, sum);
  }
  return maximum;
}

// Independent long-double Gauss-Jordan oracle, not another LAPACK estimator.
std::array<Wide, 64> Inverse(TestContext& test, std::array<Wide, 64> matrix,
                             asc::extent_t n) {
  std::array<Wide, 64> inverse{};
  const auto original = matrix;
  for (asc::extent_t j = 0; j < n; ++j) {
    inverse[j * n + j] = 1;
  }
  for (asc::extent_t column = 0; column < n; ++column) {
    asc::extent_t pivot = column;
    for (asc::extent_t row = column + 1; row < n; ++row) {
      if (std::abs(matrix[row * n + column]) >
          std::abs(matrix[pivot * n + column])) {
        pivot = row;
      }
    }
    ASC_DENSE_TEST_CHECK(test, std::abs(matrix[pivot * n + column]) > 0);
    for (asc::extent_t j = 0; j < n; ++j) {
      std::swap(matrix[pivot * n + j], matrix[column * n + j]);
      std::swap(inverse[pivot * n + j], inverse[column * n + j]);
    }
    const Wide divisor = matrix[column * n + column];
    for (asc::extent_t j = 0; j < n; ++j) {
      matrix[column * n + j] /= divisor;
      inverse[column * n + j] /= divisor;
    }
    for (asc::extent_t row = 0; row < n; ++row) {
      if (row == column) {
        continue;
      }
      const Wide multiplier = matrix[row * n + column];
      for (asc::extent_t j = 0; j < n; ++j) {
        matrix[row * n + j] -= multiplier * matrix[column * n + j];
        inverse[row * n + j] -= multiplier * inverse[column * n + j];
      }
    }
  }
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      Wide product = 0;
      for (asc::extent_t k = 0; k < n; ++k) {
        product += original[i * n + k] * inverse[k * n + j];
      }
      ASC_DENSE_TEST_CHECK(
          test, std::abs(product - Wide{i == j ? 1.0L : 0.0L}) < 1e-12L);
    }
  }
  return inverse;
}

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  asc::extent_t n;
  asc::DenseBlasLayout layout;
  std::array<T, 100> matrix{};
  std::array<T, 34> scalar{};
  std::array<Real, 18> real{};
  alignas(16) std::array<std::byte, 80> integer{};
  std::array<T, 66> packed{};
  Real rcond = -11;

  Sample(asc::extent_t order, asc::DenseBlasLayout storage)
      : n(order), layout(storage) {
    matrix.fill(Value<T>(-71));
    scalar.fill(Value<T>(-73));
    real.fill(Real{-79});
    integer.fill(std::byte{0x5a});
    packed.fill(Value<T>(-83));
  }

  T& At(asc::extent_t i, asc::extent_t j) {
    return matrix[1 + (layout == kColumn ? j * 10 + i : i * 10 + j)];
  }
  [[nodiscard]] auto View() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        matrix.data() + 1, n, n, layout, 10,
        {matrix.data(), sizeof(matrix), kHost}));
  }
  [[nodiscard]] asc::LapackWorkspace Workspace(std::size_t integer_width = 8) {
    asc::LapackWorkspace workspace;
    workspace.regions[kScalar] = {scalar.data() + 1,
                                  static_cast<std::size_t>(n) *
                                      (asc::DenseBlasComplex<T> ? 2 : 4) *
                                      sizeof(T),
                                  kHost};
    if constexpr (asc::DenseBlasComplex<T>) {
      workspace.regions[kReal] = {
          real.data() + 1, static_cast<std::size_t>(2 * n) * sizeof(Real),
          kHost};
    } else {
      workspace.regions[kInteger] = {
          integer.data() + 8, static_cast<std::size_t>(n) * integer_width,
          kHost};
    }
    if (layout == kRow) {
      workspace.regions[kLayout] = {packed.data() + 1,
                                    static_cast<std::size_t>(n * n) * sizeof(T),
                                    kHost};
    }
    return workspace;
  }

  void Guards(TestContext& test, std::size_t integer_width = 8) const {
    ASC_DENSE_TEST_EQ(test, scalar.front(), Value<T>(-73));
    ASC_DENSE_TEST_EQ(test, scalar.back(), Value<T>(-73));
    ASC_DENSE_TEST_EQ(test, real.front(), Real{-79});
    ASC_DENSE_TEST_EQ(test, real.back(), Real{-79});
    ASC_DENSE_TEST_EQ(test, integer.front(), std::byte{0x5a});
    ASC_DENSE_TEST_EQ(test, integer.back(), std::byte{0x5a});
    ASC_DENSE_TEST_EQ(test, packed.front(), Value<T>(-83));
    ASC_DENSE_TEST_EQ(test, packed.back(), Value<T>(-83));
    const auto scalar_used =
        static_cast<std::size_t>(n) * (asc::DenseBlasComplex<T> ? 2 : 4);
    for (std::size_t i = scalar_used + 1; i < scalar.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, scalar[i], Value<T>(-73));
    }
    const auto real_used =
        asc::DenseBlasComplex<T> ? static_cast<std::size_t>(2 * n) : 0;
    for (std::size_t i = real_used + 1; i < real.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, real[i], Real{-79});
    }
    const auto integer_used = asc::DenseBlasComplex<T>
                                  ? 0
                                  : static_cast<std::size_t>(n) * integer_width;
    for (std::size_t i = integer_used + 8; i < integer.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, integer[i], std::byte{0x5a});
    }
    const auto packed_used =
        layout == kRow ? static_cast<std::size_t>(n * n) : 0;
    for (std::size_t i = packed_used + 1; i < packed.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, packed[i], Value<T>(-83));
    }
  }
};

template <typename T>
std::array<Wide, 64> PrepareFactors(
    TestContext& test, const asc::ReferenceLapackProvider& provider,
    int exponent, Sample<T>& sample) {
  const auto n = sample.n;
  std::array<T, 80> factor_storage{};
  std::array<Wide, 64> original{};
  const long double scale = std::ldexp(1.0L, exponent);
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      const T value = Value<T>(scale * (i == j ? 5 + i : 0.2L * (1 + i - j)),
                               scale * (i == j ? 0.3L : 0.1L * (i + j)));
      original[i * n + j] = ToWide(value);
      factor_storage[j * 10 + i] = value;
    }
  }
  const auto factors = Take(asc::DenseBlasMatrixView<T>::Create(
      factor_storage.data(), n, n, kColumn, 10,
      {factor_storage.data(), sizeof(factor_storage), kHost}));
  std::array<asc::index_t, 8> pivots{};
  const auto pivot_view = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data(), n, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto factor_plan =
      Take(asc::QueryGetrfWorkspace(provider, factors, pivot_view));
  alignas(16) std::array<std::byte, 64> pivot_work{};
  asc::LapackWorkspace factor_workspace;
  factor_workspace.regions[kInteger] = {pivot_work.data(), sizeof(pivot_work),
                                        kHost};
  asc::LapackReport factor_report;
  ASC_DENSE_TEST_CHECK(test,
                       asc::Getrf(provider, factors, pivot_view, factor_plan,
                                  factor_workspace, factor_report)
                           .ok());
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      sample.At(i, j) = factor_storage[j * 10 + i];
    }
  }
  return original;
}

template <typename T>
void NumericalCases(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (auto layout : {kColumn, kRow}) {
    for (auto norm : {kOne, kInfinity}) {
      for (asc::extent_t n : {1, 3, 8}) {
        for (int exponent : {-60, 0, 60}) {
          Sample<T> sample(n, layout);
          const auto original =
              PrepareFactors(test, provider, exponent, sample);
          const auto inverse = Inverse(test, original, n);
          const Real anorm = static_cast<Real>(Norm(original, n, norm));
          const long double exact_rcond =
              1 / (Norm(original, n, norm) * Norm(inverse, n, norm));
          const auto before = sample;
          const auto plan = Take(WithoutAllocation(test, [&] {
            return asc::QueryGeconWorkspace(provider, norm, sample.View(),
                                            anorm, sample.rcond);
          }));
          ASC_DENSE_TEST_EQ(test, sample.matrix, before.matrix);
          ASC_DENSE_TEST_EQ(test, sample.scalar, before.scalar);
          ASC_DENSE_TEST_EQ(test, sample.rcond, before.rcond);
          for (int repetition = 0; repetition < 2; ++repetition) {
            const auto integer_width = plan.regions[kInteger].entry_bytes;
            asc::LapackReport report;
            ASC_DENSE_TEST_CHECK(
                test,
                WithoutAllocation(test, [&] {
                  return asc::Gecon(
                      provider, norm, sample.View(), anorm, sample.rcond, plan,
                      sample.Workspace(repetition == 0 ? integer_width : 8),
                      report);
                }).ok());
            ASC_DENSE_TEST_CHECK(test, report.called_provider);
            ASC_DENSE_TEST_EQ(test, report.native_info, 0);
            ASC_DENSE_TEST_CHECK(test, report.provider == provider.identity());
            ASC_DENSE_TEST_EQ(test, report.output_validity,
                              asc::LapackOutputValidity::kComplete);
            ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
            ASC_DENSE_TEST_CHECK(
                test, sample.rcond > 0 && std::isfinite(sample.rcond));
            // A norm estimate need not equal the exact inverse norm. Verify
            // a useful bounded estimate, not an invented exactness guarantee.
            ASC_DENSE_TEST_CHECK(test, sample.rcond >= exact_rcond * 0.99L);
            ASC_DENSE_TEST_CHECK(test, sample.rcond <= exact_rcond * 5.0L);
            if (n == 1) {
              ASC_DENSE_TEST_CHECK(
                  test, std::abs(sample.rcond - Real{1}) <
                            32 * std::numeric_limits<Real>::epsilon());
            }
            ASC_DENSE_TEST_EQ(test, sample.matrix, before.matrix);
            sample.Guards(test, integer_width);
          }
        }
      }
    }
  }
}

template <typename T>
void EmptySingularAndFailure(TestContext& test,
                             const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (auto layout : {kColumn, kRow}) {
    for (auto norm : {kOne, kInfinity}) {
      for (int mode : {0, 1, 2, 3}) {
        Sample<T> sample(mode == 0 ? 0 : 1, layout);
        sample.At(0, 0) = mode == 3
                              ? Value<T>(std::numeric_limits<Real>::infinity())
                              : Value<T>(0);
        const Real anorm = mode == 2 ? Real{0} : Real{1};
        const auto before = sample.matrix;
        const auto plan = Take(asc::QueryGeconWorkspace(
            provider, norm, sample.View(), anorm, sample.rcond));
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return asc::Gecon(provider, norm, sample.View(), anorm, sample.rcond,
                            plan, sample.Workspace(), report);
        });
        ASC_DENSE_TEST_EQ(test, sample.matrix, before);
        if (mode == 0) {
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_EQ(test, sample.rcond, Real{1});
          ASC_DENSE_TEST_CHECK(
              test, !report.called_provider && !report.native_info.has_value());
        } else if (mode == 3) {
          ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
          ASC_DENSE_TEST_EQ(test, report.native_info, 1);
          ASC_DENSE_TEST_EQ(test, report.outcome,
                            asc::LapackOutcome::kAccuracyWarning);
          ASC_DENSE_TEST_EQ(test, report.output_validity,
                            asc::LapackOutputValidity::kDocumentedPartial);
        } else {
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_EQ(test, sample.rcond, Real{0});
          ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        }
        ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
        sample.Guards(test);
      }
    }
  }
}

asc::LapackPlanIdentity StaleIdentity(const asc::LapackWorkspacePlan& plan) {
  auto stale_provider = plan.identity.provider();
  stale_provider.build_sha256[0] ^= std::byte{1};
  return Take(asc::LapackPlanIdentity::Create(
      plan.identity.routine(), plan.identity.scalar(),
      std::array<asc::extent_t, 2>{3, 10},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(kOne),
                                  static_cast<std::int64_t>(kRow)},
      stale_provider));
}

template <typename T>
void Rejections(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Sample<T> sample(3, kRow);
  const auto plan = Take(asc::QueryGeconWorkspace(provider, kOne, sample.View(),
                                                  Real{1}, sample.rcond));
  const auto before = sample;
  for (int mode = 0; mode < 14; ++mode) {
    auto supplied = plan;
    auto workspace = sample.Workspace();
    auto factors = sample.View();
    auto norm = kOne;
    Real anorm = 1;
    if (mode == 0) {
      workspace.regions[kScalar] = {nullptr, 0, kHost};
    }
    if (mode == 1) {
      workspace.regions[kLayout] = {sample.packed.data() + 1, sizeof(T), kHost};
    }
    if (mode == 2) {
      workspace.regions[kScalar] = {
          reinterpret_cast<std::byte*>(sample.scalar.data()) + 1,
          16 * sizeof(T), kHost};
    }
    if (mode == 3) {
      workspace.regions[kLayout] = {sample.matrix.data(), 9 * sizeof(T), kHost};
    }
    if (mode == 4) {
      workspace.regions[kScalar] = {sample.scalar.data() + 1, 16 * sizeof(T),
                                    asc::MemorySpace::kDevice};
    }
    if (mode == 5) {
      ++supplied.regions[kScalar].minimum_entries;
    }
    if (mode == 6) {
      supplied.identity =
          Take(asc::QueryGeconWorkspace(provider, kInfinity, sample.View(),
                                        Real{1}, sample.rcond))
              .identity;
    }
    if (mode == 7) {
      norm = static_cast<asc::LapackConditionNorm>(255);
    }
    if (mode == 8) {
      anorm = -1;
    }
    if (mode == 9) {
      anorm = std::numeric_limits<Real>::quiet_NaN();
    }
    if (mode == 10) {
      anorm = std::numeric_limits<Real>::infinity();
    }
    if (mode == 11) {
      workspace.regions[kLayout] = workspace.regions[kScalar];
    }
    if (mode == 12) {
      supplied.total_byte_limit = 0;
    }
    if (mode == 13) {
      supplied.identity = StaleIdentity(plan);
    }
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = 17;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gecon(provider, norm, factors, anorm, sample.rcond, supplied,
                        workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, sample.matrix, before.matrix);
    ASC_DENSE_TEST_EQ(test, sample.scalar, before.scalar);
    ASC_DENSE_TEST_EQ(test, sample.real, before.real);
    ASC_DENSE_TEST_EQ(test, sample.integer, before.integer);
    ASC_DENSE_TEST_EQ(test, sample.packed, before.packed);
    ASC_DENSE_TEST_EQ(test, sample.rcond, before.rcond);
  }
}

template <typename T>
void DescriptorRejections(TestContext& test,
                          const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Sample<T> sample(3, kRow);
  if (provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64) {
    const auto empty_huge_ld = Take(asc::DenseBlasMatrixView<const T>::Create(
        nullptr, 0, 0, kColumn,
        static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) +
            1,
        {nullptr, 0, kHost}));
    ASC_DENSE_TEST_EQ(test,
                      asc::QueryGeconWorkspace(provider, kOne, empty_huge_ld,
                                               Real{1}, sample.rcond)
                          .status()
                          .code(),
                      asc::ErrorCode::kOverflow);
  }
  if constexpr (!asc::DenseBlasComplex<T>) {
    ASC_DENSE_TEST_EQ(test,
                      asc::QueryGeconWorkspace(provider, kOne, sample.View(),
                                               Real{1}, sample.matrix[1])
                          .status()
                          .code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  const auto nonsquare = Take(asc::DenseBlasMatrixView<const T>::Create(
      sample.matrix.data() + 1, 3, 2, kRow, 10,
      {sample.matrix.data(), sizeof(sample.matrix), kHost}));
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryGeconWorkspace(provider, kOne, nonsquare, Real{1}, sample.rcond)
          .status()
          .code(),
      asc::ErrorCode::kShape);
  const auto device = Take(asc::DenseBlasMatrixView<const T>::Create(
      sample.matrix.data() + 1, 3, 3, kRow, 10,
      {sample.matrix.data(), sizeof(sample.matrix),
       asc::MemorySpace::kDevice}));
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryGeconWorkspace(provider, kOne, device, Real{1}, sample.rcond)
          .status()
          .code(),
      asc::ErrorCode::kMemoryAccess);
  const auto plan = Take(asc::QueryGeconWorkspace(provider, kOne, sample.View(),
                                                  Real{1}, sample.rcond));
  Real* aliased_output = nullptr;
  if constexpr (asc::DenseBlasComplex<T>) {
    aliased_output = sample.real.data() + 1;
  } else {
    aliased_output = sample.scalar.data() + 1;
  }
  const auto before_scalar = sample.scalar;
  const auto before_real = sample.real;
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return asc::Gecon(
                                            provider, kOne, sample.View(),
                                            Real{1}, *aliased_output, plan,
                                            sample.Workspace(), report);
                                      })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, sample.scalar, before_scalar);
  ASC_DENSE_TEST_EQ(test, sample.real, before_real);
}

template <typename T>
void IllConditioned(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const Real epsilon = std::numeric_limits<Real>::epsilon();
  for (auto layout : {kColumn, kRow}) {
    for (auto norm : {kOne, kInfinity}) {
      Sample<T> sample(2, layout);
      sample.At(0, 0) = 1;
      sample.At(0, 1) = 0;
      sample.At(1, 0) = 0;
      sample.At(1, 1) = 4 * epsilon;
      const auto plan = Take(asc::QueryGeconWorkspace(
          provider, norm, sample.View(), Real{1}, sample.rcond));
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return asc::Gecon(
                                       provider, norm, sample.View(), Real{1},
                                       sample.rcond, plan, sample.Workspace(),
                                       report);
                                 }).ok());
      ASC_DENSE_TEST_CHECK(
          test, std::abs(sample.rcond - 4 * epsilon) < 16 * epsilon * epsilon);
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
      sample.Guards(test);
    }
  }
}

void ProviderDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using asc_lapack_test::ConditionFault;
  for (auto fault : {ConditionFault::kNegative, ConditionFault::kExcess}) {
    Sample<double> sample(1, kColumn);
    sample.At(0, 0) = 1;
    const auto plan = Take(asc::QueryGeconWorkspace(
        provider, kOne, sample.View(), 1.0, sample.rcond));
    asc::LapackReport report;
    asc_lapack_test::SetConditionFault(fault);
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gecon(provider, kOne, sample.View(), 1.0, sample.rcond, plan,
                        sample.Workspace(), report);
    });
    asc_lapack_test::SetConditionFault(ConditionFault::kNone);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    ASC_DENSE_TEST_EQ(test, report.native_info,
                      fault == ConditionFault::kNegative ? -4 : 2);
    if (fault == ConditionFault::kNegative) {
      ASC_DENSE_TEST_EQ(test, report.native_argument, 4);
    }
  }
}

template <typename T>
void EmptyLargeStride(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const asc::extent_t stride =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  for (auto layout : {kColumn, kRow}) {
    const auto factors = Take(asc::DenseBlasMatrixView<const T>::Create(
        nullptr, 0, 0, layout, stride, {nullptr, 0, kHost}));
    Real rcond = -1;
    const auto query =
        asc::QueryGeconWorkspace(provider, kOne, factors, Real{0}, rcond);
    const bool supported = layout == kRow || provider.identity().integer_abi !=
                                                 asc::LapackIntegerAbi::kLp64;
    ASC_DENSE_TEST_EQ(test, query.ok(), supported);
    if (!query.ok()) {
      ASC_DENSE_TEST_EQ(test, rcond, Real{-1});
      continue;
    }
    asc::LapackWorkspace workspace;
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Gecon(provider, kOne, factors,
                                                   Real{0}, rcond, *query,
                                                   workspace, report);
                               }).ok());
    ASC_DENSE_TEST_EQ(test, rcond, Real{1});
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    const auto changed = Take(asc::DenseBlasMatrixView<const T>::Create(
        nullptr, 0, 0, layout, stride + 1, {nullptr, 0, kHost}));
    rcond = -3;
    ASC_DENSE_TEST_EQ(test,
                      WithoutAllocation(test,
                                        [&] {
                                          return asc::Gecon(
                                              provider, kOne, changed, Real{0},
                                              rcond, *query, workspace, report);
                                        })
                          .code(),
                      asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_EQ(test, rcond, Real{-3});
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  NumericalCases<T>(test, provider);
  EmptySingularAndFailure<T>(test, provider);
  EmptyLargeStride<T>(test, provider);
  Rejections<T>(test, provider);
  DescriptorRejections<T>(test, provider);
  IllConditioned<T>(test, provider);
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
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  ProviderDefects(test, provider);
  const int result = test.Finish();
  if (result == 0) {
    std::printf("GECON %s: numerical/INFO/workspace/allocation checks passed\n",
                argv[1]);
  }
  return result;
}
