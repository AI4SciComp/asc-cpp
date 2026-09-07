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
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu.h"
#include "fault_injection.h"
#include "lu_expert_faults.h"
#include "src/dense/lapack/internal_layout.h"

namespace {
using asc_dense_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);

template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::fprintf(stderr, "Layout test setup failed: %d\n",
                 static_cast<int>(value.status().code()));
    std::abort();
  }
  return std::move(*value);
}

template <typename T>
T Value(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename Operation>
auto Observe(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe probe;
  asc_lapack_test::BeginAllocationAudit();
  auto result = operation();
  const auto calls = asc_lapack_test::EndAllocationAudit();
  ASC_DENSE_TEST_EQ(test, calls, 0U);
  ASC_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  return result;
}

template <typename T>
struct Matrix {
  int rows;
  int columns;
  int ld;
  asc::DenseBlasLayout layout;
  std::array<T, 5000> values;

  Matrix(int m, int n, asc::DenseBlasLayout order)
      : rows(m), columns(n), ld((order == kRow ? n : m) + 2), layout(order) {
    values.fill(Value<T>(-719, 53));
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return static_cast<std::size_t>(layout == kRow ? i * ld + j : j * ld + i);
  }
  T& At(int i, int j) { return values[Offset(i, j)]; }
  [[nodiscard]] const T& At(int i, int j) const { return values[Offset(i, j)]; }
  auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data(), rows, columns, layout, ld,
        {values.data(), sizeof(values), kHost}));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        values.data(), rows, columns, layout, ld,
        {values.data(), sizeof(values), kHost}));
  }
  void CheckPadding(TestContext& test) const {
    for (std::size_t i = 0; i < values.size(); ++i) {
      const auto outer = i / ld;
      const auto inner = i % ld;
      if (outer >= static_cast<std::size_t>(layout == kRow ? rows : columns) ||
          inner >= static_cast<std::size_t>(layout == kRow ? columns : rows)) {
        ASC_DENSE_TEST_EQ(test, values[i], Value<T>(-719, 53));
      }
    }
  }
};

template <typename T>
struct Scratch {
  alignas(std::max_align_t) std::array<std::byte, 1024> integers{};
  std::array<T, 15002> layout;
  std::array<T, 8194> scalar;
  Scratch() {
    integers.fill(std::byte{0x5a});
    layout.fill(Value<T>(-821, 73));
    scalar.fill(Value<T>(-823, 79));
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan,
                                 bool preferred = false) {
    asc::LapackWorkspace result;
    result.regions[kInteger] = {integers.data() + 16, integers.size() - 32,
                                kHost};
    if (plan.regions[kLayout].minimum_entries != 0) {
      result.regions[kLayout] = {
          layout.data() + 1,
          static_cast<std::size_t>(plan.regions[kLayout].minimum_entries) *
              sizeof(T),
          kHost};
    }
    const auto count = preferred ? plan.regions[kScalar].preferred_entries
                                 : plan.regions[kScalar].minimum_entries;
    if (count != 0) {
      result.regions[kScalar] = {scalar.data() + 1,
                                 static_cast<std::size_t>(count) * sizeof(T),
                                 kHost};
    }
    return result;
  }
  void Check(TestContext& test, const asc::LapackWorkspacePlan& plan,
             bool preferred = false) const {
    ASC_DENSE_TEST_EQ(test, layout.front(), Value<T>(-821, 73));
    ASC_DENSE_TEST_EQ(test, layout[plan.regions[kLayout].minimum_entries + 1],
                      Value<T>(-821, 73));
    const auto count = preferred ? plan.regions[kScalar].preferred_entries
                                 : plan.regions[kScalar].minimum_entries;
    ASC_DENSE_TEST_EQ(test, scalar.front(), Value<T>(-823, 79));
    ASC_DENSE_TEST_EQ(test, scalar[count + 1], Value<T>(-823, 79));
    for (std::size_t i = 0; i < 16; ++i) {
      ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
      ASC_DENSE_TEST_EQ(test, integers[integers.size() - i - 1],
                        std::byte{0x5a});
    }
  }
};

auto Pivots(std::array<asc::index_t, 70>& values, int count) {
  return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      values.data(), count, 1, {values.data(), sizeof(values), kHost}));
}
auto Raw(const std::array<asc::index_t, 70>& values, int count) {
  return Take(asc::RawLapackPivotView::Create(
      values.data(), count, asc::LapackFactorFamily::kLuPartialPivot,
      {values.data(), sizeof(values), kHost}));
}

template <typename T>
T Op(const Matrix<T>& matrix, int i, int j, asc::DenseBlasTranspose transpose) {
  T value = transpose == asc::DenseBlasTranspose::kNone ? matrix.At(i, j)
                                                        : matrix.At(j, i);
  if constexpr (asc::DenseBlasComplex<T>) {
    if (transpose == asc::DenseBlasTranspose::kConjugateTranspose) {
      value = std::conj(value);
    }
  }
  return value;
}

template <typename T>
void Fill(Matrix<T>& matrix, int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  const Real scale = std::ldexp(Real{1}, exponent);
  const int diagonal = matrix.rows > 5 ? 400 : 7;
  for (int i = 0; i < matrix.rows; ++i) {
    for (int j = 0; j < matrix.columns; ++j) {
      matrix.At(i, j) =
          scale * Value<T>((i == j ? diagonal : 0) + ((i * 3 + j * 2) % 5) - 2,
                           (i + 2 * j) % 3 - 1);
    }
  }
  if (matrix.rows != 0 && matrix.columns != 0) {
    matrix.At(0, 0) = T{};
  }
}

template <typename T>
void Reconstruct(TestContext& test, const Matrix<T>& original,
                 const Matrix<T>& factor,
                 const std::array<asc::index_t, 70>& pivots) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<int, 70> permutation{};
  for (int i = 0; i < factor.rows; ++i) {
    permutation[i] = i;
  }
  for (int i = 0; i < std::min(factor.rows, factor.columns); ++i) {
    ASC_DENSE_TEST_CHECK(test, pivots[i] >= i + 1 && pivots[i] <= factor.rows);
    if (pivots[i] < i + 1 || pivots[i] > factor.rows) {
      std::abort();
    }
    std::swap(permutation[i], permutation[pivots[i] - 1]);
  }
  Real error = 0;
  Real norm = 0;
  for (int i = 0; i < factor.rows; ++i) {
    for (int j = 0; j < factor.columns; ++j) {
      T sum{};
      for (int k = 0; k < std::min(factor.rows, factor.columns); ++k) {
        T lower = i == k ? T{1} : T{};
        if (i > k) {
          lower = factor.At(i, k);
        }
        sum += lower * (k <= j ? factor.At(k, j) : T{});
      }
      error = std::max(error, std::abs(sum - original.At(permutation[i], j)));
      norm = std::max(norm, std::abs(original.At(i, j)));
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, error <= 512 * std::numeric_limits<Real>::epsilon() * norm);
  factor.CheckPadding(test);
}

template <typename T>
void SolveCases(TestContext& test, const asc::ReferenceLapackProvider& provider,
                const Matrix<T>& original, const Matrix<T>& factor,
                const std::array<asc::index_t, 70>& pivots,
                const asc::LapackReport& factor_report) {
  using Real = asc::DenseBlasRealType<T>;
  const auto view = Take(asc::LapackLuFactorView<T>::Create(
      factor.ConstView(), Raw(pivots, factor.rows), factor_report));
  const auto before = factor.values;
  for (auto order : {kRow, kColumn}) {
    for (auto transpose :
         {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
          asc::DenseBlasTranspose::kConjugateTranspose}) {
      Matrix<T> rhs(factor.rows, 3, order);
      for (int i = 0; i < factor.rows; ++i) {
        for (int j = 0; j < 3; ++j) {
          rhs.At(i, j) = T{};
          for (int k = 0; k < factor.rows; ++k) {
            rhs.At(i, j) +=
                Op(original, i, k, transpose) * Value<T>(k + j + 1, j - k);
          }
        }
      }
      const auto original_rhs = rhs;
      const auto plan = Take(Observe(test, [&] {
        return asc::QueryGetrsWorkspace(provider, transpose, view, rhs.View());
      }));
      Scratch<T> scratch;
      asc::LapackReport report;
      const auto status = Observe(test, [&] {
        return asc::Getrs(provider, transpose, view, rhs.View(), plan,
                          scratch.Workspace(plan), report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
      Real residual = 0;
      Real norm = 0;
      for (int i = 0; i < factor.rows; ++i) {
        for (int j = 0; j < 3; ++j) {
          ASC_DENSE_TEST_CHECK(
              test, std::abs(rhs.At(i, j) - Value<T>(i + j + 1, j - i)) <
                        4096 * std::numeric_limits<Real>::epsilon());
          T sum{};
          for (int k = 0; k < factor.rows; ++k) {
            sum += Op(original, i, k, transpose) * rhs.At(k, j);
          }
          residual = std::max(residual, std::abs(sum - original_rhs.At(i, j)));
          norm = std::max(norm, std::abs(original_rhs.At(i, j)));
        }
      }
      ASC_DENSE_TEST_CHECK(
          test, residual <= 512 * std::numeric_limits<Real>::epsilon() * norm);
      ASC_DENSE_TEST_EQ(test, factor.values, before);
      rhs.CheckPadding(test);
      scratch.Check(test, plan);
    }
  }
}

template <typename T>
void FactorCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  for (auto shape : {std::array{3, 3}, std::array{5, 3}, std::array{3, 5},
                     std::array{67, 67}}) {
    for (auto order : {kRow, kColumn}) {
      for (int exponent : {-20, 0, 20}) {
        Matrix<T> original(shape[0], shape[1], order);
        Fill(original, exponent);
        for (int algorithm = 0; algorithm < 3; ++algorithm) {
          auto factor = original;
          std::array<asc::index_t, 70> pivots{};
          pivots.fill(-501);
          auto pivot = Pivots(pivots, std::min(shape[0], shape[1]));
          const auto plan = Take(Observe(test, [&] {
            if (algorithm == 0) {
              return asc::QueryGetrfWorkspace(provider, factor.View(), pivot);
            }
            if (algorithm == 1) {
              return asc::QueryGetrf2Workspace(provider, factor.View(), pivot);
            }
            return asc::QueryGetf2Workspace(provider, factor.View(), pivot);
          }));
          Scratch<T> scratch;
          asc::LapackReport report;
          const auto status = Observe(test, [&] {
            const auto workspace = scratch.Workspace(plan);
            if (algorithm == 0) {
              return asc::Getrf(provider, factor.View(), pivot, plan, workspace,
                                report);
            }
            if (algorithm == 1) {
              return asc::Getrf2(provider, factor.View(), pivot, plan,
                                 workspace, report);
            }
            return asc::Getf2(provider, factor.View(), pivot, plan, workspace,
                              report);
          });
          ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
          ASC_DENSE_TEST_EQ(test, report.native_info, 0);
          if (!status.ok()) {
            continue;
          }
          Reconstruct(test, original, factor, pivots);
          ASC_DENSE_TEST_EQ(test, pivots[std::min(shape[0], shape[1])], -501);
          scratch.Check(test, plan);
          if (shape[0] == 3 && shape[1] == 3) {
            SolveCases(test, provider, original, factor, pivots, report);
          }
        }
      }
    }
  }
}
template <typename T>
void InverseCases(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (int n : {3, 67}) {
    for (auto order : {kRow, kColumn}) {
      for (int exponent : {-20, 0, 20}) {
        Matrix<T> original(n, n, order);
        Fill(original, exponent);
        auto factors = original;
        std::array<asc::index_t, 70> pivots{};
        auto pivot = Pivots(pivots, n);
        const auto factor_plan =
            Take(asc::QueryGetrfWorkspace(provider, factors.View(), pivot));
        Scratch<T> factor_scratch;
        asc::LapackReport report;
        ASC_DENSE_TEST_CHECK(
            test, Observe(test, [&] {
                    return asc::Getrf(
                        provider, factors.View(), pivot, factor_plan,
                        factor_scratch.Workspace(factor_plan), report);
                  }).ok());
        const auto raw = Raw(pivots, n);
        for (bool preferred : {false, true}) {
          auto inverse = factors;
          Scratch<T> scratch;
          asc::LapackWorkspace query_storage;
          query_storage.regions[kInteger] = {scratch.integers.data() + 16,
                                             scratch.integers.size() - 32,
                                             kHost};
          const auto before = inverse.values;
          const auto plan = Take(Observe(test, [&] {
            return asc::QueryGetriWorkspace(provider, inverse.View(), raw,
                                            query_storage, report);
          }));
          ASC_DENSE_TEST_CHECK(test, report.called_provider);
          ASC_DENSE_TEST_EQ(test, report.native_info, 0);
          ASC_DENSE_TEST_EQ(test, inverse.values, before);
          ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries,
                            order == kRow ? n * n : 0);
          ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                                       return asc::Getri(
                                           provider, inverse.View(), raw, plan,
                                           scratch.Workspace(plan, preferred),
                                           report);
                                     }).ok());
          ASC_DENSE_TEST_EQ(test, report.native_info, 0);
          Real error = 0;
          Real norm_a = 0;
          Real norm_inverse = 0;
          for (int i = 0; i < n; ++i) {
            Real row_a = 0;
            Real row_inverse = 0;
            for (int j = 0; j < n; ++j) {
              row_a += std::abs(original.At(i, j));
              row_inverse += std::abs(inverse.At(i, j));
              T left{};
              T right{};
              for (int k = 0; k < n; ++k) {
                left += original.At(i, k) * inverse.At(k, j);
                right += inverse.At(i, k) * original.At(k, j);
              }
              const T expected = i == j ? T{1} : T{};
              error = std::max({error, std::abs(left - expected),
                                std::abs(right - expected)});
            }
            norm_a = std::max(norm_a, row_a);
            norm_inverse = std::max(norm_inverse, row_inverse);
          }
          ASC_DENSE_TEST_CHECK(
              test, error <= 512 * std::numeric_limits<Real>::epsilon() *
                                 norm_a * norm_inverse);
          inverse.CheckPadding(test);
          scratch.Check(test, plan, preferred);
        }
      }
    }
  }
}

template <typename T>
void DriverCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (auto a_order : {kRow, kColumn}) {
    for (auto b_order : {kRow, kColumn}) {
      for (int nrhs : {0, 1, 3}) {
        Matrix<T> original(3, 3, a_order);
        Fill(original, 0);
        auto factors = original;
        Matrix<T> rhs(3, nrhs, b_order);
        for (int i = 0; i < 3; ++i) {
          for (int j = 0; j < nrhs; ++j) {
            rhs.At(i, j) = T{};
            for (int k = 0; k < 3; ++k) {
              rhs.At(i, j) += original.At(i, k) * Value<T>(k + j + 1, j - k);
            }
          }
        }
        const auto original_rhs = rhs;
        std::array<asc::index_t, 70> pivots{};
        auto pivot = Pivots(pivots, 3);
        const auto plan = Take(Observe(test, [&] {
          return asc::QueryGesvWorkspace(provider, factors.View(), pivot,
                                         rhs.View());
        }));
        ASC_DENSE_TEST_EQ(
            test, plan.regions[kLayout].minimum_entries,
            (a_order == kRow ? 9 : 0) + (b_order == kRow ? 3 * nrhs : 0));
        Scratch<T> scratch;
        asc::LapackReport report;
        const auto status = Observe(test, [&] {
          return asc::Gesv(provider, factors.View(), pivot, rhs.View(), plan,
                           scratch.Workspace(plan), report);
        });
        ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        if (!status.ok()) {
          continue;
        }
        Reconstruct(test, original, factors, pivots);
        for (int i = 0; i < 3; ++i) {
          for (int j = 0; j < nrhs; ++j) {
            ASC_DENSE_TEST_CHECK(
                test, std::abs(rhs.At(i, j) - Value<T>(i + j + 1, j - i)) <
                          4096 * std::numeric_limits<Real>::epsilon());
            T residual = -original_rhs.At(i, j);
            Real scale = std::abs(original_rhs.At(i, j));
            for (int k = 0; k < 3; ++k) {
              residual += original.At(i, k) * rhs.At(k, j);
              scale += std::abs(original.At(i, k)) * std::abs(rhs.At(k, j));
            }
            ASC_DENSE_TEST_CHECK(
                test, std::abs(residual) <=
                          512 * std::numeric_limits<Real>::epsilon() * scale);
          }
        }
        rhs.CheckPadding(test);
        scratch.Check(test, plan);
      }
    }
  }
}

template <typename T>
void SingularCases(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  for (int algorithm = 0; algorithm < 4; ++algorithm) {
    Matrix<T> matrix(2, 2, kRow);
    matrix.At(0, 0) = T{1};
    matrix.At(1, 0) = T{2};
    matrix.At(0, 1) = T{2};
    matrix.At(1, 1) = T{4};
    const auto original = matrix;
    Matrix<T> rhs(2, 1, kRow);
    rhs.At(0, 0) = T{3};
    rhs.At(1, 0) = T{7};
    const auto before_rhs = rhs.values;
    std::array<asc::index_t, 70> pivots{};
    auto pivot = Pivots(pivots, 2);
    const auto plan = Take(Observe(test, [&] {
      if (algorithm == 0) {
        return asc::QueryGetrfWorkspace(provider, matrix.View(), pivot);
      }
      if (algorithm == 1) {
        return asc::QueryGetrf2Workspace(provider, matrix.View(), pivot);
      }
      if (algorithm == 2) {
        return asc::QueryGetf2Workspace(provider, matrix.View(), pivot);
      }
      return asc::QueryGesvWorkspace(provider, matrix.View(), pivot,
                                     rhs.View());
    }));
    Scratch<T> scratch;
    asc::LapackReport report;
    const auto status = Observe(test, [&] {
      const auto workspace = scratch.Workspace(plan);
      if (algorithm == 0) {
        return asc::Getrf(provider, matrix.View(), pivot, plan, workspace,
                          report);
      }
      if (algorithm == 1) {
        return asc::Getrf2(provider, matrix.View(), pivot, plan, workspace,
                           report);
      }
      if (algorithm == 2) {
        return asc::Getf2(provider, matrix.View(), pivot, plan, workspace,
                          report);
      }
      return asc::Gesv(provider, matrix.View(), pivot, rhs.View(), plan,
                       workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, 2);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    Reconstruct(test, original, matrix, pivots);
    ASC_DENSE_TEST_EQ(test, rhs.values, before_rhs);
    const auto before_inverse = matrix.values;
    asc::LapackWorkspace query_workspace;
    query_workspace.regions[kInteger] = {scratch.integers.data() + 16,
                                         scratch.integers.size() - 32, kHost};
    const auto inverse_plan = Take(Observe(test, [&] {
      return asc::QueryGetriWorkspace(provider, matrix.View(), Raw(pivots, 2),
                                      query_workspace, report);
    }));
    ASC_DENSE_TEST_EQ(test,
                      Observe(test,
                              [&] {
                                return asc::Getri(
                                    provider, matrix.View(), Raw(pivots, 2),
                                    inverse_plan,
                                    scratch.Workspace(inverse_plan), report);
                              })
                          .code(),
                      asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info, 2);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, matrix.values, before_inverse);
  }
}

template <typename T>
void PackingPreflight(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  Matrix<T> matrix(3, 3, kRow);
  Fill(matrix, 0);
  const auto before = matrix.values;
  std::array<asc::index_t, 70> pivots{};
  pivots.fill(-503);
  const auto before_pivots = pivots;
  auto pivot = Pivots(pivots, 3);
  const auto plan =
      Take(asc::QueryGetrfWorkspace(provider, matrix.View(), pivot));
  Scratch<T> scratch;
  for (int failure = 0; failure < 7; ++failure) {
    auto workspace = scratch.Workspace(plan);
    auto supplied = plan;
    if (failure == 0) {
      workspace.regions[kLayout] = {scratch.layout.data() + 1, 8 * sizeof(T),
                                    kHost};
    }
    if (failure == 1) {
      workspace.regions[kLayout] = {matrix.values.data(), 9 * sizeof(T), kHost};
    }
    if (failure == 2) {
      workspace.regions[kLayout] = {scratch.integers.data() + 16, 9 * sizeof(T),
                                    kHost};
    }
    if (failure == 3) {
      workspace.regions[kLayout] = {scratch.layout.data() + 1, 9 * sizeof(T),
                                    asc::MemorySpace::kDevice};
    }
    if (failure == 4) {
      supplied.regions[kLayout].minimum_entries = 8;
    }
    if (failure == 5) {
      workspace.regions[kLayout] = {
          reinterpret_cast<std::byte*>(scratch.layout.data()) + 1,
          9 * sizeof(T), kHost};
    }
    if (failure == 6) {
      workspace.regions[kLayout] = {scratch.layout.data() + 1, 9 * sizeof(T),
                                    asc::MemorySpace::kManaged};
    }
    const auto before_layout = scratch.layout;
    const auto before_integers = scratch.integers;
    asc::LapackReport report;
    const auto status = Observe(test, [&] {
      return asc::Getrf(provider, matrix.View(), pivot, supplied, workspace,
                        report);
    });
    ASC_DENSE_TEST_CHECK(
        test, !status.ok() && !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, matrix.values, before);
    ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
    ASC_DENSE_TEST_EQ(test, scratch.layout, before_layout);
    ASC_DENSE_TEST_EQ(test, scratch.integers, before_integers);
  }
}

template <typename T>
void EmptyFactors(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  // An empty matrix has no reachable elements. Its unused ASC row stride can
  // exceed LP64 without being narrowed to a foreign leading dimension.
  constexpr asc::extent_t kWideStride =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  const auto pivot = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      nullptr, 0, 1, {nullptr, 0, kHost}));
  for (auto shape : {std::array{0, 0}, std::array{0, 3}, std::array{3, 0}}) {
    const auto matrix = Take(asc::DenseBlasMatrixView<T>::Create(
        nullptr, shape[0], shape[1], kRow, kWideStride, {nullptr, 0, kHost}));
    for (int algorithm = 0; algorithm < 3; ++algorithm) {
      const auto plan = Take(Observe(test, [&] {
        if (algorithm == 0) {
          return asc::QueryGetrfWorkspace(provider, matrix, pivot);
        }
        if (algorithm == 1) {
          return asc::QueryGetrf2Workspace(provider, matrix, pivot);
        }
        return asc::QueryGetf2Workspace(provider, matrix, pivot);
      }));
      ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries, 0);
      ASC_DENSE_TEST_EQ(test, plan.regions[kInteger].minimum_entries, 0);
      asc::LapackReport report;
      const auto status = Observe(test, [&] {
        if (algorithm == 0) {
          return asc::Getrf(provider, matrix, pivot, plan, {}, report);
        }
        if (algorithm == 1) {
          return asc::Getrf2(provider, matrix, pivot, plan, {}, report);
        }
        return asc::Getf2(provider, matrix, pivot, plan, {}, report);
      });
      ASC_DENSE_TEST_CHECK(
          test, status.ok() && !report.called_provider && !report.native_info);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
    }
  }
}

template <typename T>
void EmptySquare(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  constexpr asc::extent_t kWideStride =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  const auto matrix = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, 0, 0, kRow, kWideStride, {nullptr, 0, kHost}));
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, 0, 3, kRow, kWideStride, {nullptr, 0, kHost}));
  std::array<asc::index_t, 70> pivots{};
  const auto pivot = Pivots(pivots, 0);
  asc::LapackReport report;
  const auto plan = Take(Observe(test, [&] {
    return asc::QueryGesvWorkspace(provider, matrix, pivot, rhs);
  }));
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Gesv(provider, matrix, pivot, rhs,
                                                plan, {}, report);
                             }).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  const auto immutable = Take(asc::DenseBlasMatrixView<const T>::Create(
      nullptr, 0, 0, kRow, kWideStride, {nullptr, 0, kHost}));
  const auto factor = Take(
      asc::LapackLuFactorView<T>::Create(immutable, Raw(pivots, 0), report));
  const auto solve_plan = Take(Observe(test, [&] {
    return asc::QueryGetrsWorkspace(provider, asc::DenseBlasTranspose::kNone,
                                    factor, rhs);
  }));
  ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                               return asc::Getrs(
                                   provider, asc::DenseBlasTranspose::kNone,
                                   factor, rhs, solve_plan, {}, report);
                             }).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  const auto inverse_plan = Take(Observe(test, [&] {
    return asc::QueryGetriWorkspace(provider, matrix, Raw(pivots, 0), {},
                                    report);
  }));
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, inverse_plan.regions[kLayout].minimum_entries, 0);
  Scratch<T> scratch;
  ASC_DENSE_TEST_CHECK(
      test, Observe(test, [&] {
              return asc::Getri(provider, matrix, Raw(pivots, 0), inverse_plan,
                                scratch.Workspace(inverse_plan), report);
            }).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  scratch.Check(test, inverse_plan);
}

asc::Result<asc::LapackWorkspacePlan> DefectPlan(
    int operation, const asc::ReferenceLapackProvider& provider,
    Matrix<double>& matrix, Matrix<double>& rhs,
    std::array<asc::index_t, 70>& pivots, Scratch<double>& scratch) {
  const auto pivot = Pivots(pivots, matrix.rows);
  if (operation == 0) {
    return asc::QueryGetrfWorkspace(provider, matrix.View(), pivot);
  }
  if (operation == 1) {
    return asc::QueryGetrf2Workspace(provider, matrix.View(), pivot);
  }
  if (operation == 2) {
    return asc::QueryGetf2Workspace(provider, matrix.View(), pivot);
  }
  if (operation == 3) {
    return asc::QueryGesvWorkspace(provider, matrix.View(), pivot, rhs.View());
  }
  asc::LapackWorkspace query_workspace;
  query_workspace.regions[kInteger] = {scratch.integers.data() + 16,
                                       scratch.integers.size() - 32, kHost};
  asc::LapackReport report;
  return asc::QueryGetriWorkspace(provider, matrix.View(), Raw(pivots, 3),
                                  query_workspace, report);
}

asc::Status DefectExecute(int operation,
                          const asc::ReferenceLapackProvider& provider,
                          Matrix<double>& matrix, Matrix<double>& rhs,
                          std::array<asc::index_t, 70>& pivots,
                          const asc::LapackWorkspacePlan& plan,
                          const asc::LapackWorkspace& workspace,
                          asc::LapackReport& report) {
  const auto pivot = Pivots(pivots, matrix.rows);
  if (operation == 0) {
    return asc::Getrf(provider, matrix.View(), pivot, plan, workspace, report);
  }
  if (operation == 1) {
    return asc::Getrf2(provider, matrix.View(), pivot, plan, workspace, report);
  }
  if (operation == 2) {
    return asc::Getf2(provider, matrix.View(), pivot, plan, workspace, report);
  }
  if (operation == 3) {
    return asc::Gesv(provider, matrix.View(), pivot, rhs.View(), plan,
                     workspace, report);
  }
  return asc::Getri(provider, matrix.View(), Raw(pivots, 3), plan, workspace,
                    report);
}

void PackingPostflight(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  using asc_lapack_test::ExpertFault;
  using asc_lapack_test::InjectedFault;
  constexpr std::array kOriginalFaults{InjectedFault::kNegativeInfo,
                                       InjectedFault::kExcessInfo,
                                       InjectedFault::kInvalidPivot};
  constexpr std::array kExpertFaults{ExpertFault::kNegativeInfo,
                                     ExpertFault::kExcessInfo,
                                     ExpertFault::kInvalidPivot};
  for (int operation = 0; operation < 5; ++operation) {
    // GETRI has no output pivot array; its two INFO defects are applicable.
    for (int fault = 0; fault < (operation == 4 ? 2 : 3); ++fault) {
      Matrix<double> matrix(3, 3, kRow);
      Fill(matrix, 0);
      Matrix<double> rhs(3, 2, kRow);
      Fill(rhs, 0);
      const auto before = matrix.values;
      const auto before_rhs = rhs.values;
      std::array<asc::index_t, 70> pivots{1, 2, 3};
      const auto before_pivots = pivots;
      Scratch<double> scratch;
      const auto plan =
          Take(DefectPlan(operation, provider, matrix, rhs, pivots, scratch));
      asc_lapack_test::SetInjectedFault(operation == 0 ? kOriginalFaults[fault]
                                                       : InjectedFault::kNone);
      asc_lapack_test::SetExpertFault(operation == 0 ? ExpertFault::kNone
                                                     : kExpertFaults[fault]);
      asc::LapackReport report;
      const auto status = Observe(test, [&] {
        return DefectExecute(operation, provider, matrix, rhs, pivots, plan,
                             scratch.Workspace(plan), report);
      });
      asc_lapack_test::SetInjectedFault(InjectedFault::kNone);
      asc_lapack_test::SetExpertFault(ExpertFault::kNone);
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_EQ(test, matrix.values, before);
      ASC_DENSE_TEST_EQ(test, rhs.values, before_rhs);
      ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
      scratch.Check(test, plan);
    }
  }
}

void PackingCountArithmetic(TestContext& test,
                            const asc::ReferenceLapackProvider& provider) {
  const auto matrix = Take(asc::DenseBlasMatrixView<float>::Create(
      nullptr, 0, 0, kRow, 1, {nullptr, 0, kHost}));
  const auto pivot = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      nullptr, 0, 1, {nullptr, 0, kHost}));
  auto plan = Take(asc::QueryGetrfWorkspace(provider, matrix, pivot));
  // Pure generated-plan arithmetic: no fabricated large memory span, foreign
  // query, dereference or attempted allocation is used to exercise size_t.
  constexpr auto kMaximum = std::numeric_limits<asc::extent_t>::max();
  plan.regions[kScalar] = {kMaximum, kMaximum, 1, 1};
  plan.regions[kInteger] = {kMaximum, kMaximum, 1, 1};
  ASC_DENSE_TEST_CHECK(test,
                       Observe(test, [&] {
                         return asc::internal_lapack_layout::CheckTotal(plan);
                       }).ok());
  plan.regions[kLayout] = {2, 2, 1, 1};
  ASC_DENSE_TEST_EQ(
      test,
      Observe(test,
              [&] { return asc::internal_lapack_layout::CheckTotal(plan); })
          .code(),
      asc::ErrorCode::kOverflow);
}

template <typename T>
void Numerics(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  FactorCases<T>(test, provider);
  InverseCases<T>(test, provider);
  DriverCases<T>(test, provider);
  SingularCases<T>(test, provider);
  PackingPreflight<T>(test, provider);
  EmptyFactors<T>(test, provider);
  EmptySquare<T>(test, provider);
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
    PackingPostflight(test, provider);
    PackingCountArithmetic(test, provider);
  } else if (scalar == "c") {
    Numerics<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Numerics<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
