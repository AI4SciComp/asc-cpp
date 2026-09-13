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
#include <type_traits>
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
#include "asc/dense/providers/lapack_lu.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_expert_faults.h"

namespace {
using asc_dense_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);

template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::fprintf(stderr, "Unexpected test setup failure: code=%d %s\n",
                 static_cast<int>(value.status().code()),
                 value.status().message().c_str());
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
auto Matrix(std::array<T, Size>& values, asc::extent_t rows,
            asc::extent_t columns, asc::extent_t ld,
            asc::DenseBlasLayout layout = kColumn) {
  return Take(asc::DenseBlasMatrixView<T>::Create(
      values.data(), rows, columns, layout, ld,
      {values.data(), values.size() * sizeof(T), kHost}));
}
template <std::size_t Size>
auto Pivots(std::array<asc::index_t, Size>& values, asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      values.data(), count, 1,
      {values.data(), values.size() * sizeof(asc::index_t), kHost}));
}
template <std::size_t Size>
auto Raw(const std::array<asc::index_t, Size>& values, asc::extent_t count) {
  return Take(asc::RawLapackPivotView::Create(
      values.data(), count, asc::LapackFactorFamily::kLuPartialPivot,
      {values.data(), values.size() * sizeof(asc::index_t), kHost}));
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
struct Scratch {
  alignas(std::max_align_t) std::array<std::byte, 1024> integers{};
  std::array<T, 8194> scalars{};
  Scratch() {
    integers.fill(std::byte{0x5a});
    scalars.fill(Value<T>(-719, 51));
  }
  asc::LapackWorkspace Workspace(std::size_t entries = 0) {
    asc::LapackWorkspace result;
    result.regions[kInteger] = {integers.data() + 16, integers.size() - 32,
                                kHost};
    if (entries != 0) {
      result.regions[kScalar] = {scalars.data() + 1, entries * sizeof(T),
                                 kHost};
    }
    return result;
  }
  void CheckGuards(TestContext& test, std::size_t entries) const {
    for (std::size_t i = 0; i < 16; ++i) {
      ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
      ASC_DENSE_TEST_EQ(test, integers[integers.size() - i - 1],
                        std::byte{0x5a});
    }
    ASC_DENSE_TEST_EQ(test, scalars[0], Value<T>(-719, 51));
    for (std::size_t i = entries + 1; i < scalars.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, scalars[i], Value<T>(-719, 51));
    }
  }
};

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  int m;
  int n;
  int ld;
  std::array<T, 5000> values;
  std::array<T, 5000> original;
  std::array<asc::index_t, 70> pivots;
  Sample(int rows, int columns, Real scale)
      : m(rows), n(columns), ld(rows + 2) {
    values.fill(Value<T>(-503, 23));
    pivots.fill(-619);
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < m; ++i) {
        values[j * ld + i] =
            scale *
            Value<T>((i == j ? 3 * std::max(m, n) : 0) + (i + 3 * j) % 5 - 2,
                     (2 * i + j) % 3 - 1);
      }
    }
    // Force a nontrivial first pivot without making the square case ill-scaled.
    for (int j = 0; j < n; ++j) {
      std::swap(values[j * ld], values[j * ld + m - 1]);
    }
    original = values;
  }
  void Padding(TestContext& test) const {
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i / ld >= static_cast<std::size_t>(n) ||
          i % ld >= static_cast<std::size_t>(m)) {
        ASC_DENSE_TEST_EQ(test, values[i], original[i]);
      }
    }
    for (std::size_t i = std::min(m, n); i < pivots.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, pivots[i], -619);
    }
  }
  void Reconstruction(TestContext& test) const {
    auto permuted = original;
    for (int i = 0; i < std::min(m, n); ++i) {
      ASC_DENSE_TEST_CHECK(test, pivots[i] >= i + 1 && pivots[i] <= m);
      for (int j = 0; j < n; ++j) {
        std::swap(permuted[j * ld + i],
                  permuted[j * ld + static_cast<int>(pivots[i]) - 1]);
      }
    }
    Real error = 0;
    Real norm = 0;
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < m; ++i) {
        T sum{};
        for (int k = 0; k < std::min(m, n); ++k) {
          T l{};
          if (i == k) {
            l = T{1};
          } else if (i > k) {
            l = values[k * ld + i];
          }
          const T u = k <= j ? values[j * ld + k] : T{};
          sum += l * u;
        }
        error = std::max(error, std::abs(sum - permuted[j * ld + i]));
        norm = std::max(norm, std::abs(original[j * ld + i]));
      }
    }
    ASC_DENSE_TEST_CHECK(
        test, error <= 40 * std::max(m, n) *
                           std::numeric_limits<Real>::epsilon() * norm);
    Padding(test);
  }
};

template <typename T>
void FactorCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const bool recursive : {true, false}) {
    for (const auto dimensions : {std::array{3, 3}, std::array{5, 3},
                                  std::array{3, 5}, std::array{67, 67}}) {
      for (const int exponent : {-20, 0, 20}) {
        Sample<T> sample(dimensions[0], dimensions[1],
                         std::ldexp(Real{1}, exponent));
        const auto matrix =
            Matrix(sample.values, sample.m, sample.n, sample.ld);
        const auto pivots = Pivots(sample.pivots, std::min(sample.m, sample.n));
        auto queried = WithoutAllocation(test, [&] {
          return recursive ? asc::QueryGetrf2Workspace(provider, matrix, pivots)
                           : asc::QueryGetf2Workspace(provider, matrix, pivots);
        });
        const auto plan = Take(std::move(queried));
        Scratch<T> scratch;
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return recursive ? asc::Getrf2(provider, matrix, pivots, plan,
                                         scratch.Workspace(), report)
                           : asc::Getf2(provider, matrix, pivots, plan,
                                        scratch.Workspace(), report);
        });
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        ASC_DENSE_TEST_EQ(test, report.factor_family,
                          asc::LapackFactorFamily::kLuPartialPivot);
        sample.Reconstruction(test);
        scratch.CheckGuards(test, 0);
      }
    }
  }
}

template <typename T>
void InverseProduct(TestContext& test, const Sample<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  Real error = 0;
  for (bool reverse : {false, true}) {
    for (int j = 0; j < sample.n; ++j) {
      for (int i = 0; i < sample.n; ++i) {
        T sum{};
        for (int k = 0; k < sample.n; ++k) {
          sum += reverse ? sample.values[k * sample.ld + i] *
                               sample.original[j * sample.ld + k]
                         : sample.original[k * sample.ld + i] *
                               sample.values[j * sample.ld + k];
        }
        error = std::max(error, std::abs(sum - (i == j ? T{1} : T{})));
      }
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, error <= 80 * sample.n * std::numeric_limits<Real>::epsilon());
}

template <typename T>
void InverseCases(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const int n : {3, 67}) {
    for (const int exponent : {-20, 0, 20}) {
      Sample<T> sample(n, n, std::ldexp(Real{1}, exponent));
      const auto matrix = Matrix(sample.values, n, n, sample.ld);
      const auto pivots = Pivots(sample.pivots, n);
      const auto factor_plan =
          Take(asc::QueryGetrf2Workspace(provider, matrix, pivots));
      Scratch<T> scratch;
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return asc::Getrf2(
                                       provider, matrix, pivots, factor_plan,
                                       scratch.Workspace(), report);
                                 }).ok());
      const auto factored = sample.values;
      const auto saved_pivots = sample.pivots;
      const auto raw = Raw(sample.pivots, n);
      auto queried = WithoutAllocation(test, [&] {
        return asc::QueryGetriWorkspace(provider, matrix, raw,
                                        scratch.Workspace(), report);
      });
      const auto plan = Take(std::move(queried));
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnchanged);
      ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
      ASC_DENSE_TEST_CHECK(
          test, std::string_view(report.routine.data()).ends_with(".query"));
      ASC_DENSE_TEST_EQ(test, sample.values, factored);
      ASC_DENSE_TEST_EQ(test, sample.pivots, saved_pivots);
      ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].minimum_entries, n);
      ASC_DENSE_TEST_CHECK(test, plan.regions[kScalar].preferred_entries >= n);
      scratch.CheckGuards(test, 0);
      for (const auto entries : {plan.regions[kScalar].minimum_entries,
                                 plan.regions[kScalar].preferred_entries}) {
        sample.values = factored;
        Scratch<T> execution_scratch;
        const auto query_count = asc_lapack_test::InverseQueryCalls();
        const auto execute_count = asc_lapack_test::InverseExecutionCalls();
        const auto status = WithoutAllocation(test, [&] {
          return asc::Getri(provider, matrix, raw, plan,
                            execution_scratch.Workspace(entries), report);
        });
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
        ASC_DENSE_TEST_EQ(test, asc_lapack_test::InverseQueryCalls(),
                          query_count);
        if constexpr (std::is_same_v<T, double>) {
          ASC_DENSE_TEST_EQ(test, asc_lapack_test::InverseExecutionCalls(),
                            execute_count + 1);
        }
        InverseProduct(test, sample);
        sample.Padding(test);
        ASC_DENSE_TEST_EQ(test, sample.pivots, saved_pivots);
        execution_scratch.CheckGuards(test, entries);
      }
    }
  }
}

template <typename T>
void DriverCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const int nrhs : {0, 1, 3}) {
    Sample<T> sample(5, 5, Real{1});
    const auto matrix = Matrix(sample.values, 5, 5, sample.ld);
    const auto pivots = Pivots(sample.pivots, 5);
    std::array<T, 32> rhs;
    rhs.fill(Value<T>(-337, 19));
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < 5; ++i) {
        T sum{};
        for (int k = 0; k < 5; ++k) {
          sum +=
              sample.original[k * sample.ld + i] * Value<T>(k + j - 2, j - k);
        }
        rhs[j * 7 + i] = sum;
      }
    }
    const auto original_rhs = rhs;
    const auto b = Matrix(rhs, 5, nrhs, 7);
    const auto plan = Take(WithoutAllocation(test, [&] {
      return asc::QueryGesvWorkspace(provider, matrix, pivots, b);
    }));
    Scratch<T> scratch;
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gesv(provider, matrix, pivots, b, plan, scratch.Workspace(),
                       report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    sample.Reconstruction(test);
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < 5; ++i) {
        ASC_DENSE_TEST_CHECK(
            test, std::abs(rhs[j * 7 + i] - Value<T>(i + j - 2, j - i)) <=
                      300 * std::numeric_limits<Real>::epsilon());
        T product{};
        for (int k = 0; k < 5; ++k) {
          product += sample.original[k * sample.ld + i] * rhs[j * 7 + k];
        }
        ASC_DENSE_TEST_CHECK(test,
                             std::abs(product - original_rhs[j * 7 + i]) <=
                                 3000 * std::numeric_limits<Real>::epsilon());
      }
    }
    for (std::size_t i = 0; i < rhs.size(); ++i) {
      if (i / 7 >= static_cast<std::size_t>(nrhs) || i % 7 >= 5) {
        ASC_DENSE_TEST_EQ(test, rhs[i], original_rhs[i]);
      }
    }
    scratch.CheckGuards(test, 0);
  }
}

template <typename T>
void SingularCases(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  for (const bool recursive : {true, false}) {
    std::array<T, 9> values{T{1}, T{}, T{}, T{}, T{}, T{}, T{}, T{}, T{2}};
    std::array<asc::index_t, 3> pivots{};
    const auto matrix = Matrix(values, 3, 3, 3);
    const auto pivot = Pivots(pivots, 3);
    const auto plan =
        Take(recursive ? asc::QueryGetrf2Workspace(provider, matrix, pivot)
                       : asc::QueryGetf2Workspace(provider, matrix, pivot));
    Scratch<T> scratch;
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return recursive ? asc::Getrf2(provider, matrix, pivot, plan,
                                     scratch.Workspace(), report)
                       : asc::Getf2(provider, matrix, pivot, plan,
                                    scratch.Workspace(), report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info, 2);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
    ASC_DENSE_TEST_EQ(test, pivots, (std::array<asc::index_t, 3>{1, 2, 3}));
    const auto raw = Raw(pivots, 3);
    const auto inverse_plan = Take(WithoutAllocation(test, [&] {
      return asc::QueryGetriWorkspace(provider, matrix, raw,
                                      scratch.Workspace(), report);
    }));
    const auto singular_before = values;
    ASC_DENSE_TEST_EQ(test,
                      WithoutAllocation(test,
                                        [&] {
                                          return asc::Getri(
                                              provider, matrix, raw,
                                              inverse_plan,
                                              scratch.Workspace(3), report);
                                        })
                          .code(),
                      asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info, 2);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, values, singular_before);
    std::array<T, 3> rhs{T{1}, T{2}, T{3}};
    const auto b = Matrix(rhs, 3, 1, 3);
    const auto driver_plan =
        Take(asc::QueryGesvWorkspace(provider, matrix, pivot, b));
    const auto original_rhs = rhs;
    ASC_DENSE_TEST_EQ(test,
                      WithoutAllocation(test,
                                        [&] {
                                          return asc::Gesv(
                                              provider, matrix, pivot, b,
                                              driver_plan, scratch.Workspace(),
                                              report);
                                        })
                          .code(),
                      asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, rhs, original_rhs);
    ASC_DENSE_TEST_EQ(test, report.native_info, 2);
  }
}

template <typename T>
void EmptyCases(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  std::array<T, 0> empty{};
  std::array<asc::index_t, 0> no_pivots{};
  const auto matrix = Matrix(empty, 0, 0, 1);
  const auto pivots = Pivots(no_pivots, 0);
  Scratch<T> scratch;
  asc::LapackReport report;
  auto plan = Take(asc::QueryGetrf2Workspace(provider, matrix, pivots));
  ASC_DENSE_TEST_CHECK(test, asc::Getrf2(provider, matrix, pivots, plan,
                                         scratch.Workspace(), report)
                                 .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  plan = Take(asc::QueryGetf2Workspace(provider, matrix, pivots));
  ASC_DENSE_TEST_CHECK(test, asc::Getf2(provider, matrix, pivots, plan,
                                        scratch.Workspace(), report)
                                 .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  plan = Take(asc::QueryGesvWorkspace(provider, matrix, pivots, matrix));
  ASC_DENSE_TEST_CHECK(test, asc::Gesv(provider, matrix, pivots, matrix, plan,
                                       scratch.Workspace(), report)
                                 .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  const auto raw = Raw(no_pivots, 0);
  plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryGetriWorkspace(provider, matrix, raw, scratch.Workspace(),
                                    report);
  }));
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].minimum_entries, 1);
  ASC_DENSE_TEST_CHECK(test, asc::Getri(provider, matrix, raw, plan,
                                        scratch.Workspace(1), report)
                                 .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
}

void WorkspacePreflight(TestContext& test,
                        const asc::ReferenceLapackProvider& provider) {
  std::array<double, 16> values{2, 1, 0, 0, 0, 3};
  std::array<asc::index_t, 2> pivots{1, 2};
  const auto matrix = Matrix(values, 2, 2, 4);
  const auto raw = Raw(pivots, 2);
  Scratch<double> scratch;
  asc::LapackReport report;
  const auto plan = Take(asc::QueryGetriWorkspace(provider, matrix, raw,
                                                  scratch.Workspace(), report));
  const auto before = values;
  const auto before_pivots = pivots;
  for (int failure = 0; failure < 10; ++failure) {
    auto workspace = scratch.Workspace(2);
    auto invalid_plan = plan;
    if (failure == 0) {
      workspace.regions[kInteger] = {nullptr, 0, kHost};
    } else if (failure == 1) {
      workspace.regions[kScalar] = {scratch.scalars.data(), sizeof(double),
                                    kHost};
    } else if (failure == 2) {
      workspace.regions[kInteger] = {scratch.integers.data() + 1, 64, kHost};
    } else if (failure == 3) {
      workspace.regions[kScalar] = {values.data(), sizeof(values), kHost};
    } else if (failure == 4) {
      workspace.regions[kInteger] = {pivots.data(), sizeof(pivots), kHost};
    } else if (failure == 5) {
      workspace.regions[kScalar] = {scratch.scalars.data(), 32,
                                    asc::MemorySpace::kDevice};
    } else if (failure == 6) {
      ++invalid_plan.regions[kScalar].preferred_entries;
    } else if (failure == 7) {
      ++invalid_plan.regions[kScalar].minimum_entries;
    } else if (failure == 8) {
      invalid_plan.total_byte_limit = 0;
    } else {
      workspace.regions[kScalar] = {scratch.integers.data() + 16, 32, kHost};
    }
    const auto bytes = scratch.integers;
    const auto scalars = scratch.scalars;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Getri(provider, matrix, raw, invalid_plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, values, before);
    ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
    ASC_DENSE_TEST_EQ(test, scratch.integers, bytes);
    ASC_DENSE_TEST_EQ(test, scratch.scalars, scalars);
    if (failure == 0 || failure == 2 || failure == 3 || failure == 4 ||
        failure == 5 || failure == 9) {
      auto queried = WithoutAllocation(test, [&] {
        return asc::QueryGetriWorkspace(provider, matrix, raw, workspace,
                                        report);
      });
      ASC_DENSE_TEST_CHECK(test, !queried.ok());
      ASC_DENSE_TEST_CHECK(test,
                           !report.called_provider && !report.native_info);
      ASC_DENSE_TEST_EQ(test, values, before);
      ASC_DENSE_TEST_EQ(test, scratch.integers, bytes);
    }
  }
}

void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  std::array<double, 16> values{2, 1, 0, 0, 0, 3};
  std::array<asc::index_t, 2> pivots{1, 2};
  const auto matrix = Matrix(values, 2, 2, 4);
  const auto raw = Raw(pivots, 2);
  Scratch<double> scratch;
  asc::LapackReport report;
  const auto plan = Take(asc::QueryGetriWorkspace(provider, matrix, raw,
                                                  scratch.Workspace(), report));
  const auto before = values;
  const auto before_pivots = pivots;
  pivots[1] = 1;
  ASC_DENSE_TEST_CHECK(test,
                       !asc::QueryGetriWorkspace(provider, matrix, raw,
                                                 scratch.Workspace(), report)
                            .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  pivots = before_pivots;
  const auto row_major =
      Matrix(values, 2, 2, 4, asc::DenseBlasLayout::kRowMajor);
  const auto row_plan = Take(asc::QueryGetriWorkspace(
      provider, row_major, raw, scratch.Workspace(), report));
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test,
                    row_plan
                        .regions[static_cast<std::size_t>(
                            asc::LapackWorkspaceKind::kLayoutConversion)]
                        .minimum_entries,
                    4);
  ASC_DENSE_TEST_CHECK(test, !asc::Getri(provider, row_major, raw, row_plan,
                                         scratch.Workspace(2), report)
                                  .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  const auto stale = Matrix(values, 2, 2, 5);
  ASC_DENSE_TEST_EQ(
      test,
      asc::Getri(provider, stale, raw, plan, scratch.Workspace(2), report)
          .code(),
      asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_EQ(test, values, before);
}

void FactorDriverPreflight(TestContext& test,
                           const asc::ReferenceLapackProvider& provider) {
  std::array<double, 16> values{2, 1, 0, 0, 0, 3};
  std::array<double, 8> rhs{1, 2};
  std::array<asc::index_t, 2> pivots{-1, -1};
  const auto matrix = Matrix(values, 2, 2, 4);
  const auto b = Matrix(rhs, 2, 1, 4);
  const auto pivot = Pivots(pivots, 2);
  Scratch<double> scratch;
  asc::LapackReport report;
  for (int operation = 0; operation < 3; ++operation) {
    auto plan = Take(asc::QueryGesvWorkspace(provider, matrix, pivot, b));
    if (operation == 0) {
      plan = Take(asc::QueryGetrf2Workspace(provider, matrix, pivot));
    } else if (operation == 1) {
      plan = Take(asc::QueryGetf2Workspace(provider, matrix, pivot));
    }
    for (int defect = 0; defect < 5; ++defect) {
      auto invalid_plan = plan;
      auto workspace = scratch.Workspace();
      if (defect == 0) {
        workspace.regions[kInteger] = {nullptr, 0, kHost};
      } else if (defect == 1) {
        workspace.regions[kInteger] = {values.data(), sizeof(values), kHost};
      } else if (defect == 2) {
        workspace.regions[kInteger] = {scratch.integers.data() + 1, 32, kHost};
      } else if (defect == 3) {
        ++invalid_plan.regions[kInteger].minimum_entries;
      } else {
        invalid_plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
      }
      const auto before = values;
      const auto before_rhs = rhs;
      const auto before_pivots = pivots;
      const auto before_workspace = scratch.integers;
      const auto status = WithoutAllocation(test, [&] {
        if (operation == 0) {
          return asc::Getrf2(provider, matrix, pivot, invalid_plan, workspace,
                             report);
        }
        if (operation == 1) {
          return asc::Getf2(provider, matrix, pivot, invalid_plan, workspace,
                            report);
        }
        return asc::Gesv(provider, matrix, pivot, b, invalid_plan, workspace,
                         report);
      });
      ASC_DENSE_TEST_CHECK(test, !status.ok());
      ASC_DENSE_TEST_CHECK(test,
                           !report.called_provider && !report.native_info);
      ASC_DENSE_TEST_EQ(test, values, before);
      ASC_DENSE_TEST_EQ(test, rhs, before_rhs);
      ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
      ASC_DENSE_TEST_EQ(test, scratch.integers, before_workspace);
    }
  }
}

void DriverDescriptorRejections(TestContext& test,
                                const asc::ReferenceLapackProvider& provider) {
  std::array<double, 16> a{};
  std::array<double, 16> b{};
  std::array<asc::index_t, 4> p{};
  const auto matrix = Matrix(a, 2, 2, 4);
  const auto rhs = Matrix(b, 2, 1, 4);
  const auto pivots = Pivots(p, 2);
  const auto row_major = Matrix(a, 2, 2, 4, asc::DenseBlasLayout::kRowMajor);
  for (const auto& row_plan :
       {Take(asc::QueryGetrf2Workspace(provider, row_major, pivots)),
        Take(asc::QueryGetf2Workspace(provider, row_major, pivots)),
        Take(asc::QueryGesvWorkspace(provider, row_major, pivots, rhs))}) {
    ASC_DENSE_TEST_EQ(test,
                      row_plan
                          .regions[static_cast<std::size_t>(
                              asc::LapackWorkspaceKind::kLayoutConversion)]
                          .minimum_entries,
                      4);
  }
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryGesvWorkspace(provider, matrix, pivots, matrix).status().code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryGesvWorkspace(provider, Matrix(a, 2, 3, 4), pivots, rhs)
          .status()
          .code(),
      asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryGesvWorkspace(provider, matrix, pivots, Matrix(b, 3, 1, 4))
          .status()
          .code(),
      asc::ErrorCode::kShape);
  const auto strided = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      p.data(), 2, 2, {p.data(), sizeof(p), kHost}));
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryGesvWorkspace(provider, matrix, strided, rhs).status().code(),
      asc::ErrorCode::kInvalidArgument);
}

void ProviderDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using asc_lapack_test::ExpertFault;
  std::array<double, 4> a{2, 1, 0, 3};
  std::array<double, 2> b{1, 2};
  std::array<asc::index_t, 2> pivots{1, 2};
  const auto matrix = Matrix(a, 2, 2, 2);
  const auto rhs = Matrix(b, 2, 1, 2);
  const auto pivot = Pivots(pivots, 2);
  const auto raw = Raw(pivots, 2);
  Scratch<double> scratch;
  asc::LapackReport report;
  const auto inverse_plan = Take(asc::QueryGetriWorkspace(
      provider, matrix, raw, scratch.Workspace(), report));
  for (auto fault : {ExpertFault::kQueryNan, ExpertFault::kQueryInfinity,
                     ExpertFault::kQueryNegative, ExpertFault::kQueryShort,
                     ExpertFault::kNegativeInfo, ExpertFault::kExcessInfo}) {
    asc_lapack_test::SetExpertFault(fault);
    auto queried = WithoutAllocation(test, [&] {
      return asc::QueryGetriWorkspace(provider, matrix, raw,
                                      scratch.Workspace(), report);
    });
    asc_lapack_test::SetExpertFault(ExpertFault::kNone);
    ASC_DENSE_TEST_CHECK(test, !queried.ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, a, (std::array<double, 4>{2, 1, 0, 3}));
  }
  for (int operation = 0; operation < 4; ++operation) {
    auto plan = inverse_plan;
    if (operation == 0) {
      plan = Take(asc::QueryGetrf2Workspace(provider, matrix, pivot));
    } else if (operation == 1) {
      plan = Take(asc::QueryGetf2Workspace(provider, matrix, pivot));
    } else if (operation == 2) {
      plan = Take(asc::QueryGesvWorkspace(provider, matrix, pivot, rhs));
    }
    for (auto fault : {ExpertFault::kNegativeInfo, ExpertFault::kExcessInfo,
                       ExpertFault::kInvalidPivot}) {
      if (operation == 3 && fault == ExpertFault::kInvalidPivot) {
        continue;  // GETRI has no output pivot array to corrupt.
      }
      a = {2, 1, 0, 3};
      pivots = {1, 2};
      asc_lapack_test::SetExpertFault(fault);
      const auto status = WithoutAllocation(test, [&] {
        if (operation == 0) {
          return asc::Getrf2(provider, matrix, pivot, plan, scratch.Workspace(),
                             report);
        }
        if (operation == 1) {
          return asc::Getf2(provider, matrix, pivot, plan, scratch.Workspace(),
                            report);
        }
        if (operation == 2) {
          return asc::Gesv(provider, matrix, pivot, rhs, plan,
                           scratch.Workspace(), report);
        }
        return asc::Getri(provider, matrix, raw, plan, scratch.Workspace(2),
                          report);
      });
      asc_lapack_test::SetExpertFault(ExpertFault::kNone);
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_EQ(test, pivots, (std::array<asc::index_t, 2>{1, 2}));
      if (fault == ExpertFault::kNegativeInfo) {
        const auto argument = std::array{4, 4, 7, 6}[operation];
        ASC_DENSE_TEST_EQ(test, report.native_info, -argument);
        ASC_DENSE_TEST_EQ(test, report.native_argument, argument);
      }
    }
  }
}

template <typename T>
void ForeignCursorPreflight(TestContext& test,
                            const asc::ReferenceLapackProvider& provider) {
  const asc::extent_t limit =
      provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
          ? std::numeric_limits<std::int32_t>::max()
          : std::numeric_limits<std::int64_t>::max();
  // These are genuinely backed two-element, one-column matrices. The huge
  // unused column stride consumes no storage, yet GETF2's possible row swap
  // increments its provider-INTEGER vector cursor by LDA even for N=1.
  std::array<T, 2> values{Value<T>(1), Value<T>(2)};
  const auto original = values;
  std::array<asc::index_t, 1> pivot_values{-1};
  const auto matrix = Matrix(values, 2, 1, limit);
  const auto pivots = Pivots(pivot_values, 1);
  const auto rejected = WithoutAllocation(
      test, [&] { return asc::QueryGetf2Workspace(provider, matrix, pivots); });
  ASC_DENSE_TEST_EQ(test, rejected.status().code(), asc::ErrorCode::kOverflow);
  const auto safe_plan = Take(asc::QueryGetf2Workspace(
      provider, Matrix(values, 2, 1, limit - 1), pivots));
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Getf2(provider, matrix, pivots, safe_plan, {}, report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, values == original);
  ASC_DENSE_TEST_EQ(test, pivot_values[0], -1);
  // Recursive/blocked GETRF use scalar swaps for this one-column case.
  // Do not impose GETF2's vector-stride restriction on those distinct paths.
  ASC_DENSE_TEST_CHECK(
      test, asc::QueryGetrf2Workspace(provider, matrix, pivots).ok());
  ASC_DENSE_TEST_CHECK(test,
                       asc::QueryGetrfWorkspace(provider, matrix, pivots).ok());
  ASC_DENSE_TEST_CHECK(
      test, asc::QueryGetf2Workspace(provider, Matrix(values, 2, 1, limit - 1),
                                     pivots)
                .ok());
}

template <typename T>
void Numerics(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  ForeignCursorPreflight<T>(test, provider);
  FactorCases<T>(test, provider);
  InverseCases<T>(test, provider);
  DriverCases<T>(test, provider);
  SingularCases<T>(test, provider);
  EmptyCases<T>(test, provider);
  std::puts(
      "GETRF2/GETF2: square/tall/wide/67-square, three scales, P*A=L*U; "
      "GETRI: left/right inverse, minimum/preferred WORK, no requery; "
      "GESV: 0/1/3 RHS solve/residual; singular/empty/allocation/guards");
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
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
  WorkspacePreflight(test, provider);
  Preflight(test, provider);
  FactorDriverPreflight(test, provider);
  DriverDescriptorRejections(test, provider);
  ProviderDefects(test, provider);
  return test.Finish();
}
