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
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_equilibration_faults.h"

namespace {
using asc_dense_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::fprintf(stderr, "Unexpected setup failure: %d %s\n",
                 static_cast<int>(value.status().code()),
                 value.status().message().c_str());
    std::abort();
  }
  return std::move(*value);
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
T Value(double real, double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return T{static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
asc::DenseBlasRealType<T> Magnitude(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return std::abs(value.real()) + std::abs(value.imag());
  } else {
    return std::abs(value);
  }
}

template <typename Real>
Real Quantize(Real value) {
  // Independent binary decomposition, not the provider's LOG/RADIX**INT path.
  int exponent = 0;
  const Real fraction = std::frexp(value, &exponent);
  --exponent;
  if (value < Real{1} && fraction != Real{0.5}) {
    ++exponent;  // INT(log2(x)) truncates toward zero, including x<1.
  }
  return std::ldexp(Real{1}, exponent);
}

template <typename T, std::size_t Size>
auto Vector(std::array<T, Size>& values, asc::extent_t count,
            std::size_t offset = 1, asc::stride_t increment = 1) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + offset, count, increment,
      {values.data(), values.size() * sizeof(T), kHost}));
}

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasLayout layout;
  std::array<T, 24> matrix;
  std::array<T, 12> scratch;
  std::array<Real, 5> rows;
  std::array<Real, 4> columns;
  asc::LapackEquilibrationStatistics<Real> statistics{-31, -37, -41};

  Sample(asc::DenseBlasLayout order, int exponent) : layout(order) {
    matrix.fill(Value<T>(-709, 29));
    scratch.fill(Value<T>(-719, 31));
    rows.fill(Real{-53});
    columns.fill(Real{-59});
    const Real scale = std::ldexp(Real{1}, exponent);
    At(0, 0) = scale * Value<T>(3, 4);
    At(1, 0) = scale * Value<T>(5, -2);
    At(2, 0) = scale * Value<T>(12, 1);
    At(0, 1) = scale * Value<T>(0.5, 0.25);
    At(1, 1) = scale * Value<T>(0.25, 0.25);
    At(2, 1) = scale * Value<T>(1, -0.5);
  }
  T& At(int i, int j) {
    return matrix[layout == kColumn ? 5 * j + i : 4 * i + j];
  }
  [[nodiscard]] auto Matrix() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        matrix.data(), 3, 2, layout, layout == kColumn ? 5 : 4,
        {matrix.data(), sizeof(matrix), kHost}));
  }
  auto Workspace() {
    asc::LapackWorkspace result;
    if (layout == kRow) {
      result.regions[kLayout] = {scratch.data() + 1, 6 * sizeof(T), kHost};
    }
    return result;
  }
  auto Query(const asc::ReferenceLapackProvider& provider, bool radix) {
    return radix
               ? asc::QueryGeequbWorkspace(provider, Matrix(), Vector(rows, 3),
                                           Vector(columns, 2), statistics)
               : asc::QueryGeequWorkspace(provider, Matrix(), Vector(rows, 3),
                                          Vector(columns, 2), statistics);
  }
  auto Execute(const asc::ReferenceLapackProvider& provider, bool radix,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::LapackReport& report) {
    return radix ? asc::Geequb(provider, Matrix(), Vector(rows, 3),
                               Vector(columns, 2), statistics, plan, workspace,
                               report)
                 : asc::Geequ(provider, Matrix(), Vector(rows, 3),
                              Vector(columns, 2), statistics, plan, workspace,
                              report);
  }
};

template <typename Real>
void Near(TestContext& test, Real actual, Real expected) {
  if (std::abs(actual - expected) >
      16 * std::numeric_limits<Real>::epsilon() *
          std::max(std::abs(expected), std::numeric_limits<Real>::min())) {
    std::fprintf(stderr, "Scale diagnostic: actual=%a expected=%a\n",
                 static_cast<double>(actual), static_cast<double>(expected));
  }
  ASC_DENSE_TEST_CHECK(
      test,
      std::abs(actual - expected) <=
          16 * std::numeric_limits<Real>::epsilon() *
              std::max(std::abs(expected), std::numeric_limits<Real>::min()));
}

template <typename T>
void ExpectedScales(TestContext& test, Sample<T>& sample, bool radix) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<Real, 3> row_max{};
  std::array<Real, 3> expected_rows{};
  std::array<Real, 2> column_max{};
  const Real safe_minimum = std::numeric_limits<Real>::min();
  const Real safe_maximum = Real{1} / safe_minimum;
  for (int i = 0; i < 3; ++i) {
    row_max[i] =
        std::max(Magnitude(sample.At(i, 0)), Magnitude(sample.At(i, 1)));
    if (radix) {
      row_max[i] = Quantize(row_max[i]);
    }
    expected_rows[i] =
        Real{1} / std::clamp(row_max[i], safe_minimum, safe_maximum);
    Near(test, sample.rows[i + 1], expected_rows[i]);
  }
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 3; ++i) {
      column_max[j] = std::max(column_max[j],
                               Magnitude(sample.At(i, j)) * expected_rows[i]);
    }
    if (radix) {
      column_max[j] = Quantize(column_max[j]);
    }
    Near(test, sample.columns[j + 1],
         Real{1} / std::clamp(column_max[j], safe_minimum, safe_maximum));
  }
  const auto [minimum_row, maximum_row] =
      std::minmax_element(row_max.begin(), row_max.end());
  const auto [minimum_column, maximum_column] =
      std::minmax_element(column_max.begin(), column_max.end());
  Near(test, sample.statistics.row_condition,
       std::clamp(*minimum_row, safe_minimum, safe_maximum) /
           std::min(*maximum_row, safe_maximum));
  Near(test, sample.statistics.column_condition,
       std::clamp(*minimum_column, safe_minimum, safe_maximum) /
           std::min(*maximum_column, safe_maximum));
  Near(test, sample.statistics.absolute_maximum, *maximum_row);
}

template <typename T>
void NumericalCases(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr bool kSingle = std::numeric_limits<Real>::digits == 24;
  for (const bool radix : {false, true}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const int exponent : {-20, 0, 20, kSingle ? 124 : 1020}) {
        Sample<T> sample(layout, exponent);
        const auto before = sample.matrix;
        const auto before_scratch = sample.scratch;
        const auto before_rows = sample.rows;
        const auto before_columns = sample.columns;
        const auto plan = Take(WithoutAllocation(
            test, [&] { return sample.Query(provider, radix); }));
        ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries,
                          layout == kRow ? 6 : 0);
        ASC_DENSE_TEST_EQ(test, sample.matrix, before);
        ASC_DENSE_TEST_EQ(test, sample.scratch, before_scratch);
        ASC_DENSE_TEST_EQ(test, sample.rows, before_rows);
        ASC_DENSE_TEST_EQ(test, sample.columns, before_columns);
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return sample.Execute(provider, radix, plan, sample.Workspace(),
                                report);
        });
        if (!status.ok()) {
          std::fprintf(
              stderr,
              "Equilibration diagnostic: radix=%d layout=%d exponent=%d "
              "INFO=%lld rows=%a,%a,%a\n",
              radix, static_cast<int>(layout), exponent,
              static_cast<long long>(report.native_info.value_or(-999)),
              static_cast<double>(sample.rows[1]),
              static_cast<double>(sample.rows[2]),
              static_cast<double>(sample.rows[3]));
        }
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kComplete);
        ExpectedScales(test, sample, radix);
        ASC_DENSE_TEST_EQ(test, sample.matrix, before);
        ASC_DENSE_TEST_EQ(test, sample.rows.front(), before_rows.front());
        ASC_DENSE_TEST_EQ(test, sample.rows.back(), before_rows.back());
        ASC_DENSE_TEST_EQ(test, sample.columns.front(), before_columns.front());
        ASC_DENSE_TEST_EQ(test, sample.columns.back(), before_columns.back());
        for (std::size_t i = 0; i < sample.scratch.size(); ++i) {
          if (layout == kColumn || i == 0 || i > 6) {
            ASC_DENSE_TEST_EQ(test, sample.scratch[i], before_scratch[i]);
          }
        }
      }
    }
  }
}

template <typename T>
void PinnedSubnormalBehavior(TestContext& test,
                             const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr int kExponent =
      std::numeric_limits<Real>::digits == 24 ? -140 : -1050;
  for (const auto layout : {kColumn, kRow}) {
    for (bool radix : {false, true}) {
      Sample<T> sample(layout, kExponent);
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
          ASC_DENSE_TEST_CHECK(test, Magnitude(sample.At(i, j)) > 0);
        }
      }
      const auto original = sample.matrix;
      const auto plan = Take(sample.Query(provider, radix));
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return sample.Execute(provider, radix, plan, sample.Workspace(),
                              report);
      });
      const bool known_failure = radix && provider.identity().integer_abi ==
                                              asc::LapackIntegerAbi::kLp64;
      if (known_failure) {
        // A direct pinned-upstream probe reproduces this same nonzero fixture.
        // This asserts fidelity/failure reporting, not mathematical success.
        // The failed mathematical-success gate remains in the durable review.
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_EQ(test, report.native_info, 1);
        ASC_DENSE_TEST_EQ(test, report.outcome,
                          asc::LapackOutcome::kPartialResult);
        ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 0);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kDocumentedPartial);
        for (int i = 1; i <= 3; ++i) {
          ASC_DENSE_TEST_EQ(test, sample.rows[i], Real{0});
        }
        ASC_DENSE_TEST_EQ(test, sample.columns[1], Real{-59});
        ASC_DENSE_TEST_EQ(test, sample.columns[2], Real{-59});
        ASC_DENSE_TEST_EQ(test, sample.statistics.row_condition, Real{-31});
        ASC_DENSE_TEST_EQ(test, sample.statistics.column_condition, Real{-37});
        ASC_DENSE_TEST_EQ(test, sample.statistics.absolute_maximum, Real{0});
      } else {
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        ExpectedScales(test, sample, radix);
      }
      ASC_DENSE_TEST_EQ(test, sample.matrix, original);
    }
  }
}

template <typename T>
void ZeroRowsAndColumns(TestContext& test,
                        const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const bool radix : {false, true}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool zero_row : {true, false}) {
        Sample<T> sample(layout, 0);
        if (zero_row) {
          sample.At(1, 0) = T{};
          sample.At(1, 1) = T{};
        } else {
          for (int i = 0; i < 3; ++i) {
            sample.At(i, 1) = T{};
          }
        }
        const auto before = sample.matrix;
        const auto plan = Take(sample.Query(provider, radix));
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return sample.Execute(provider, radix, plan, sample.Workspace(),
                                report);
        });
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_EQ(test, report.native_info, zero_row ? 2 : 5);
        ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kDocumentedPartial);
        ASC_DENSE_TEST_EQ(test, sample.matrix, before);
        ASC_DENSE_TEST_EQ(test, sample.statistics.column_condition, Real{-37});
        if (zero_row) {
          ASC_DENSE_TEST_EQ(test, sample.statistics.row_condition, Real{-31});
          ASC_DENSE_TEST_EQ(test, sample.columns[1], Real{-59});
          ASC_DENSE_TEST_EQ(test, sample.columns[2], Real{-59});
        } else {
          ASC_DENSE_TEST_CHECK(test, sample.statistics.row_condition > 0);
          for (int i = 0; i < 3; ++i) {
            ASC_DENSE_TEST_CHECK(test, sample.rows[i + 1] > 0);
          }
        }
      }
    }
  }
}

template <typename T>
void EmptyCases(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto shape :
       {std::array{0, 0}, std::array{0, 2}, std::array{3, 0}}) {
    for (const auto layout : {kColumn, kRow}) {
      const auto matrix = Take(asc::DenseBlasMatrixView<const T>::Create(
          nullptr, shape[0], shape[1], layout, 4, {nullptr, 0, kHost}));
      std::array<Real, 5> rows{-3, -3, -3, -3, -3};
      std::array<Real, 4> columns{-5, -5, -5, -5};
      asc::LapackEquilibrationStatistics<Real> statistics{};
      asc::LapackReport report;
      for (bool radix : {false, true}) {
        const auto plan =
            Take(radix ? asc::QueryGeequbWorkspace(
                             provider, matrix, Vector(rows, shape[0]),
                             Vector(columns, shape[1]), statistics)
                       : asc::QueryGeequWorkspace(
                             provider, matrix, Vector(rows, shape[0]),
                             Vector(columns, shape[1]), statistics));
        const auto status = WithoutAllocation(test, [&] {
          return radix ? asc::Geequb(provider, matrix, Vector(rows, shape[0]),
                                     Vector(columns, shape[1]), statistics,
                                     plan, {}, report)
                       : asc::Geequ(provider, matrix, Vector(rows, shape[0]),
                                    Vector(columns, shape[1]), statistics, plan,
                                    {}, report);
        });
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_CHECK(test,
                             !report.called_provider && !report.native_info);
        ASC_DENSE_TEST_EQ(test, statistics.row_condition, Real{1});
        ASC_DENSE_TEST_EQ(test, statistics.column_condition, Real{1});
        ASC_DENSE_TEST_EQ(test, statistics.absolute_maximum, Real{0});
        ASC_DENSE_TEST_EQ(test, rows,
                          (std::array<Real, 5>{-3, -3, -3, -3, -3}));
        ASC_DENSE_TEST_EQ(test, columns, (std::array<Real, 4>{-5, -5, -5, -5}));
      }
    }
  }
}

template <typename T>
void EmptyStrideCase(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     std::array<int, 2> shape, asc::DenseBlasLayout layout,
                     bool radix) {
  using Real = asc::DenseBlasRealType<T>;
  const asc::extent_t stride =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  const auto matrix = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, shape[0], shape[1], layout, stride, {nullptr, 0, kHost}));
  std::array<Real, 5> rows{-3, -3, -3, -3, -3};
  std::array<Real, 4> columns{-5, -5, -5, -5};
  asc::LapackEquilibrationStatistics<Real> statistics{-31, -37, -41};
  const auto query = WithoutAllocation(test, [&] {
    return radix ? asc::QueryGeequbWorkspace(
                       provider, matrix, Vector(rows, shape[0]),
                       Vector(columns, shape[1]), statistics)
                 : asc::QueryGeequWorkspace(
                       provider, matrix, Vector(rows, shape[0]),
                       Vector(columns, shape[1]), statistics);
  });
  const bool supported = layout == kRow || provider.identity().integer_abi !=
                                               asc::LapackIntegerAbi::kLp64;
  ASC_DENSE_TEST_EQ(test, query.ok(), supported);
  ASC_DENSE_TEST_EQ(test, statistics.row_condition, Real{-31});
  ASC_DENSE_TEST_EQ(test, statistics.column_condition, Real{-37});
  ASC_DENSE_TEST_EQ(test, statistics.absolute_maximum, Real{-41});
  ASC_DENSE_TEST_EQ(test, rows, (std::array<Real, 5>{-3, -3, -3, -3, -3}));
  ASC_DENSE_TEST_EQ(test, columns, (std::array<Real, 4>{-5, -5, -5, -5}));
  if (!query.ok()) {
    ASC_DENSE_TEST_EQ(test, query.status().code(), asc::ErrorCode::kOverflow);
    return;
  }
  asc::LapackReport report;
  const auto execute = [&](asc::DenseBlasMatrixView<const T> view) {
    return WithoutAllocation(test, [&] {
      return radix ? asc::Geequb(provider, view, Vector(rows, shape[0]),
                                 Vector(columns, shape[1]), statistics, *query,
                                 {}, report)
                   : asc::Geequ(provider, view, Vector(rows, shape[0]),
                                Vector(columns, shape[1]), statistics, *query,
                                {}, report);
    });
  };
  ASC_DENSE_TEST_CHECK(test, execute(matrix).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, statistics.row_condition, Real{1});
  ASC_DENSE_TEST_EQ(test, statistics.column_condition, Real{1});
  ASC_DENSE_TEST_EQ(test, statistics.absolute_maximum, Real{0});
  const auto changed = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, shape[0], shape[1], layout, stride + 1, {nullptr, 0, kHost}));
  statistics = {-31, -37, -41};
  ASC_DENSE_TEST_EQ(test, execute(changed).code(),
                    asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, statistics.row_condition, Real{-31});
  ASC_DENSE_TEST_EQ(test, statistics.column_condition, Real{-37});
  ASC_DENSE_TEST_EQ(test, statistics.absolute_maximum, Real{-41});
  ASC_DENSE_TEST_EQ(test, rows, (std::array<Real, 5>{-3, -3, -3, -3, -3}));
  ASC_DENSE_TEST_EQ(test, columns, (std::array<Real, 4>{-5, -5, -5, -5}));
}

template <typename T>
void EmptyLargeStride(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  for (const auto shape :
       {std::array{0, 0}, std::array{0, 2}, std::array{3, 0}}) {
    for (const auto layout : {kColumn, kRow}) {
      for (bool radix : {false, true}) {
        EmptyStrideCase<T>(test, provider, shape, layout, radix);
      }
    }
  }
}

void WorkspaceRejections(TestContext& test,
                         const asc::ReferenceLapackProvider& provider) {
  Sample<double> sample(kRow, 0);
  for (bool radix : {false, true}) {
    const auto plan = Take(sample.Query(provider, radix));
    for (int defect = 0; defect < 7; ++defect) {
      auto supplied = plan;
      auto workspace = sample.Workspace();
      if (defect == 0) {
        workspace = {};
      } else if (defect == 1) {
        workspace.regions[kLayout] = {sample.matrix.data(), 48, kHost};
      } else if (defect == 2) {
        workspace.regions[kLayout] = {sample.scratch.data(), 40, kHost};
      } else if (defect == 3) {
        workspace.regions[kLayout] = {
            reinterpret_cast<std::byte*>(sample.scratch.data()) + 1, 48, kHost};
      } else if (defect == 4) {
        ++supplied.regions[kLayout].preferred_entries;
      } else if (defect == 5) {
        supplied = Take(sample.Query(provider, !radix));
      } else {
        workspace.regions[kLayout] = {sample.scratch.data(), 48,
                                      asc::MemorySpace::kDevice};
      }
      const auto before_matrix = sample.matrix;
      const auto before_rows = sample.rows;
      const auto before_columns = sample.columns;
      const auto before_scratch = sample.scratch;
      const auto before_stats = sample.statistics;
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return sample.Execute(provider, radix, supplied, workspace, report);
      });
      ASC_DENSE_TEST_CHECK(test, !status.ok());
      ASC_DENSE_TEST_CHECK(test,
                           !report.called_provider && !report.native_info);
      ASC_DENSE_TEST_EQ(test, sample.matrix, before_matrix);
      ASC_DENSE_TEST_EQ(test, sample.rows, before_rows);
      ASC_DENSE_TEST_EQ(test, sample.columns, before_columns);
      ASC_DENSE_TEST_EQ(test, sample.scratch, before_scratch);
      ASC_DENSE_TEST_EQ(test, sample.statistics.row_condition,
                        before_stats.row_condition);
      ASC_DENSE_TEST_EQ(test, sample.statistics.column_condition,
                        before_stats.column_condition);
      ASC_DENSE_TEST_EQ(test, sample.statistics.absolute_maximum,
                        before_stats.absolute_maximum);
    }
  }
}

template <typename T>
void Numerics(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  NumericalCases<T>(test, provider);
  PinnedSubnormalBehavior<T>(test, provider);
  ZeroRowsAndColumns<T>(test, provider);
  EmptyCases<T>(test, provider);
  EmptyLargeStride<T>(test, provider);
  std::puts(
      "GEEQU/GEEQUB: column/row-major, independent reciprocal/radix scales, "
      "quantized AMAX, zero rows/columns and partial validity, empty, "
      "explicit packing, guards and allocation probes passed");
}
void DescriptorRejections(TestContext& test,
                          const asc::ReferenceLapackProvider& provider) {
  Sample<double> sample(kColumn, 0);
  const auto query = [&](asc::DenseBlasVectorView<double> rows,
                         asc::DenseBlasVectorView<double> columns) {
    return WithoutAllocation(test, [&] {
      return asc::QueryGeequWorkspace(provider, sample.Matrix(), rows, columns,
                                      sample.statistics);
    });
  };
  ASC_DENSE_TEST_EQ(
      test,
      query(Vector(sample.rows, 2), Vector(sample.columns, 2)).status().code(),
      asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(
      test,
      query(Vector(sample.rows, 3), Vector(sample.rows, 2)).status().code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(
      test,
      query(Vector(sample.matrix, 3, 0), Vector(sample.columns, 2))
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(
      test,
      query(Vector(sample.rows, 3), Vector(sample.columns, 2, 0, 2))
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
  const auto device_rows = Take(asc::DenseBlasVectorView<double>::Create(
      sample.rows.data(), 3, 1,
      {sample.rows.data(), sizeof(sample.rows), asc::MemorySpace::kDevice}));
  ASC_DENSE_TEST_EQ(
      test, query(device_rows, Vector(sample.columns, 2)).status().code(),
      asc::ErrorCode::kMemoryAccess);
  const auto scalar = Take(asc::DenseBlasMatrixView<const double>::Create(
      &sample.statistics.row_condition, 1, 1, kColumn, 1,
      {&sample.statistics, sizeof(sample.statistics), kHost}));
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryGeequWorkspace(provider, scalar, Vector(sample.rows, 1),
                               Vector(sample.columns, 1), sample.statistics)
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
}

void ProviderDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using asc_lapack_test::EquilibrationFault;
  for (bool radix : {false, true}) {
    for (auto fault :
         {EquilibrationFault::kNegative, EquilibrationFault::kExcess}) {
      Sample<double> sample(kColumn, 0);
      const auto plan = Take(sample.Query(provider, radix));
      asc::LapackReport report;
      asc_lapack_test::SetEquilibrationFault(fault);
      const auto status = WithoutAllocation(test, [&] {
        return sample.Execute(provider, radix, plan, sample.Workspace(),
                              report);
      });
      asc_lapack_test::SetEquilibrationFault(EquilibrationFault::kNone);
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      if (fault == EquilibrationFault::kNegative) {
        ASC_DENSE_TEST_EQ(test, report.native_info, -4);
        ASC_DENSE_TEST_EQ(test, report.native_argument, 4);
      } else {
        ASC_DENSE_TEST_EQ(test, report.native_info, 6);
      }
    }
  }
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
  WorkspaceRejections(test, provider);
  DescriptorRejections(test, provider);
  ProviderDefects(test, provider);
  return test.Finish();
}
