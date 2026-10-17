#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook.h"
#include "asc/dense/providers/lapack_indefinite_rook_driver.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_indefinite_rook_test::Adjoint;
using asc_indefinite_rook_test::EqualBytes;
using asc_indefinite_rook_test::kColumn;
using asc_indefinite_rook_test::kLower;
using asc_indefinite_rook_test::kRow;
using asc_indefinite_rook_test::kUpper;
using asc_indefinite_rook_test::Matrix;
using asc_indefinite_rook_test::Pivots;
using asc_indefinite_rook_test::Scratch;
using asc_indefinite_rook_test::Take;
using asc_indefinite_rook_test::TestContext;
using asc_indefinite_rook_test::ToWide;
using asc_indefinite_rook_test::Value;
using asc_indefinite_rook_test::Wide;
using asc_indefinite_rook_test::WithoutAllocation;

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> a,
           asc::DenseBlasVectorView<asc::index_t> pivots,
           asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHesvRookWorkspace(provider, triangle, a, pivots, b);
    }
  }
  return asc::QuerySysvRookWorkspace(provider, triangle, a, pivots, b);
}

template <typename T>
asc::Status Driver(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   asc::DenseBlasMatrixView<T> a,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::DenseBlasMatrixView<T> b,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HesvRook(provider, triangle, a, pivots, b, plan, workspace,
                           report);
    }
  }
  return asc::SysvRook(provider, triangle, a, pivots, b, plan, workspace,
                       report);
}

std::size_t Offset(int i, int j, asc::DenseBlasLayout layout, int ld) {
  return 1U +
         static_cast<std::size_t>(layout == kColumn ? j * ld + i : i * ld + j);
}

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  std::array<T, 5000> a{};
  std::array<T, 5000> original{};
  std::array<asc::index_t, 72> pivots{};
  std::array<Wide, 4489> full{};

  Sample(int order, bool he, asc::DenseBlasTriangle tri,
         asc::DenseBlasLayout storage, int exponent, bool singular)
      : n(order), hermitian(he), triangle(tri), layout(storage) {
    a.fill(Value<T>(-503, 29));
    pivots.fill(-509);
    const Real scale = std::ldexp(Real{1}, exponent);
    auto permutation = [order](int i) {
      if (order > 3) {
        if (i == 1) {
          return order - 1;
        }
        if (i == order - 1) {
          return 1;
        }
      }
      return i;
    };
    for (int j = 0; j < n; ++j) {
      for (int i = j; i < n; ++i) {
        const int p = permutation(i);
        const int q = permutation(j);
        const int low = std::min(p, q);
        const int high = std::max(p, q);
        T value{};
        if (p == q) {
          value = Value<T>(p % 3 == 0 || p == n - 1 ? 4 : 0,
                           hermitian ? 0 : 0.125L);
        } else if (low % 3 == 1 && high == low + 1) {
          value = Value<T>(2, 0.5L);
        } else {
          value = Value<T>(static_cast<long double>((p + q) % 3 - 1) / 128,
                           static_cast<long double>((p + q) % 5 - 2) / 256);
        }
        if (order == 3) {
          // Force the rook search to leave the current column and choose an
          // internal 2x2 block, requiring BOTH independent interchanges.
          const bool strong = triangle == kUpper ? high == 1 : low == 1;
          long double magnitude = 1;
          if (strong) {
            magnitude = 4;
          } else if (high - low == 2) {
            magnitude = 0.5L;
          }
          value = p == q ? T{} : Value<T>(magnitude, 0.125L);
        }
        value *= scale;
        const Wide wide = ToWide(value);
        full[i * n + j] = wide;
        full[j * n + i] = Adjoint(wide, hermitian);
      }
    }
    if (singular) {
      for (int i = 0; i < n; ++i) {
        full[i * n + n - 1] = Wide{};
        full[(n - 1) * n + i] = Wide{};
      }
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (Selected(i, j)) {
          a[Offset(i, j)] =
              Value<T>(full[i * n + j].real(), full[i * n + j].imag());
          if constexpr (asc::DenseBlasComplex<T>) {
            if (hermitian && i == j) {
              a[Offset(i, j)].imag(std::numeric_limits<Real>::quiet_NaN());
            }
          }
        } else {
          a[Offset(i, j)] = Value<T>(std::numeric_limits<Real>::quiet_NaN());
        }
      }
    }
    original = a;
  }

  [[nodiscard]] int Ld() const { return n + 2; }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1U + (layout == kColumn ? static_cast<std::size_t>(j) * Ld() + i
                                   : static_cast<std::size_t>(i) * Ld() + j);
  }
  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == kUpper ? i <= j : i >= j;
  }
  auto View() { return Matrix(a, n, n, layout, Ld()); }
  [[nodiscard]] auto ConstView() const { return Matrix(a, n, n, layout, Ld()); }
  [[nodiscard]] Wide Entry(int i, int j) const {
    return ToWide(a[Offset(i, j)]);
  }

  void Guards(TestContext& test) const {
    for (std::size_t k = 0; k < a.size(); ++k) {
      const auto relative = static_cast<int>(k) - 1;
      const int i = layout == kColumn ? relative % Ld() : relative / Ld();
      const int j = layout == kColumn ? relative / Ld() : relative % Ld();
      if (k == 0 || i >= n || j >= n || !Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test, EqualBytes(&a[k], &original[k], sizeof(T)));
      }
    }
    ASC_DENSE_TEST_EQ(test, pivots.front(), -509);
    for (std::size_t i = static_cast<std::size_t>(n) + 1; i < pivots.size();
         ++i) {
      ASC_DENSE_TEST_EQ(test, pivots[i], -509);
    }
  }

  void UndoInterchanges(std::array<Wide, 4489>& reconstructed, int first,
                        int last, bool paired) const {
    // Undo the second interchange before the first. Each negative partner
    // specifies its own simultaneous row/column interchange.
    for (int step = 0; step < last - first; ++step) {
      const int swapped = triangle == kLower ? last - 1 - step : first + step;
      const auto raw = pivots[static_cast<std::size_t>(swapped) + 1];
      const int target = static_cast<int>(paired ? -raw : raw) - 1;
      for (int j = 0; j < n; ++j) {
        std::swap(reconstructed[swapped * n + j],
                  reconstructed[target * n + j]);
      }
      for (int i = 0; i < n; ++i) {
        std::swap(reconstructed[i * n + swapped],
                  reconstructed[i * n + target]);
      }
    }
  }

  void Reconstruction(TestContext& test) const {
    // Independent reverse Schur-complement reconstruction. Restore a complete
    // 1x1/2x2 block, multiply stored multipliers by D, add LDU/LDL* updates,
    // then undo its simultaneous row/column interchange. No LAPACK solve or
    // source implementation is used as the oracle.
    std::array<Wide, 4489> reconstructed{};
    int cursor = triangle == kLower ? n - 1 : 0;
    while (cursor >= 0 && cursor < n) {
      const bool paired = pivots[static_cast<std::size_t>(cursor) + 1] < 0;
      const int size = paired ? 2 : 1;
      const int first = triangle == kLower ? cursor - size + 1 : cursor;
      const int last = first + size;
      std::array<Wide, 4> d{};
      d[0] = Entry(first, first);
      if (paired) {
        d[3] = Entry(first + 1, first + 1);
        if (triangle == kLower) {
          d[2] = Entry(first + 1, first);
          d[1] = Adjoint(d[2], hermitian);
        } else {
          d[1] = Entry(first, first + 1);
          d[2] = Adjoint(d[1], hermitian);
        }
      }
      const int begin_active = triangle == kLower ? last : 0;
      const int end_active = triangle == kLower ? n : first;
      for (int i = begin_active; i < end_active; ++i) {
        for (int j = begin_active; j < end_active; ++j) {
          for (int p = 0; p < size; ++p) {
            for (int q = 0; q < size; ++q) {
              reconstructed[i * n + j] +=
                  Entry(i, first + p) * d[2 * p + q] *
                  Adjoint(Entry(j, first + q), hermitian);
            }
          }
        }
        for (int p = 0; p < size; ++p) {
          Wide value{};
          for (int q = 0; q < size; ++q) {
            value += Entry(i, first + q) * d[2 * q + p];
          }
          reconstructed[i * n + first + p] = value;
          reconstructed[(first + p) * n + i] = Adjoint(value, hermitian);
        }
      }
      for (int p = 0; p < size; ++p) {
        for (int q = 0; q < size; ++q) {
          reconstructed[(first + p) * n + first + q] = d[2 * p + q];
        }
      }
      UndoInterchanges(reconstructed, first, last, paired);
      cursor += triangle == kLower ? -size : size;
    }
    long double error = 0;
    long double norm = 0;
    for (int i = 0; i < n * n; ++i) {
      ASC_DENSE_TEST_CHECK(test, std::isfinite(reconstructed[i].real()) &&
                                     std::isfinite(reconstructed[i].imag()));
      error = std::max(error, std::abs(reconstructed[i] - full[i]));
      norm = std::max(norm, std::abs(full[i]));
    }
    const auto tolerance =
        32 * std::max(n, 1) * std::numeric_limits<Real>::epsilon() * norm;
    if (!(error <= tolerance)) {
      std::fprintf(stderr,
                   "reconstruction n=%d he=%d triangle=%d layout=%d "
                   "error=%Lg limit=%Lg\n",
                   n, hermitian, static_cast<int>(triangle),
                   static_cast<int>(layout), error, tolerance);
    }
    ASC_DENSE_TEST_CHECK(test, error <= tolerance);
    Guards(test);
  }
};

Wide Expected(int row, int column) {
  return {static_cast<long double>((row + column) % 3 - 1),
          static_cast<long double>((row + 2 * column) % 5 - 2) / 4};
}

template <typename T>
void Solution(TestContext& test, const Sample<T>& sample,
              const std::array<T, 500>& b, const std::array<T, 500>& saved_b,
              asc::DenseBlasLayout rhs_layout, int nrhs, int ldb) {
  using Real = asc::DenseBlasRealType<T>;
  long double residual = 0;
  long double denominator = 0;
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      const auto expected = Expected(i, j);
      const Wide x = ToWide(Value<T>(expected.real(), expected.imag()));
      const Wide actual = ToWide(b[Offset(i, j, rhs_layout, ldb)]);
      ASC_DENSE_TEST_CHECK(
          test, std::abs(actual - x) <=
                    256 * sample.n * std::numeric_limits<Real>::epsilon());
      Wide product{};
      const Wide original = ToWide(saved_b[Offset(i, j, rhs_layout, ldb)]);
      long double bound = std::abs(original);
      for (int k = 0; k < sample.n; ++k) {
        const Wide coefficient = sample.full[i * sample.n + k];
        const Wide solution = ToWide(b[Offset(k, j, rhs_layout, ldb)]);
        product += coefficient * solution;
        bound += std::abs(coefficient) * std::abs(solution);
      }
      residual = std::max(residual, std::abs(product - original));
      denominator = std::max(denominator, bound);
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, residual <= 128 * std::max(sample.n, 1) *
                            std::numeric_limits<Real>::epsilon() * denominator);
}

template <typename T>
void Reuse(TestContext& test, const asc::ReferenceLapackProvider& provider,
           const Sample<T>& sample, std::array<T, 500>& b,
           const std::array<T, 500>& saved_b,
           asc::DenseBlasMatrixView<T> b_view, const asc::LapackReport& report,
           int nrhs, bool singular) {
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  const auto factor = asc::ReferenceRookFactorView<T>::Create(
      provider, sample.ConstView(), sample.triangle,
      sample.hermitian ? asc_indefinite_rook_test::kHermitian
                       : asc_indefinite_rook_test::kSymmetric,
      asc_indefinite_rook_test::Raw(sample.pivots, sample.n), report);
  ASC_DENSE_TEST_EQ(test, factor.ok(), !singular);
  if (factor.ok()) {
    // The actual driver origin is retained and accepted by a subsequent solve.
    ASC_DENSE_TEST_CHECK(test,
                         factor->originating_routine().ends_with("sv_rook"));
    const auto factors_before = sample.a;
    const auto pivots_before = sample.pivots;
    const auto solution = b;
    b = saved_b;
    const auto reuse_plan = Take(asc_indefinite_rook_test::QuerySolve(
        provider, sample.hermitian, *factor, b_view));
    Scratch<T> reuse_scratch;
    const auto reuse_workspace = reuse_scratch.Workspace(reuse_plan);
    asc::LapackReport reuse_report;
    const auto reuse = WithoutAllocation(test, [&] {
      return asc_indefinite_rook_test::Solve(provider, sample.hermitian,
                                             *factor, b_view, reuse_plan,
                                             reuse_workspace, reuse_report);
    });
    ASC_DENSE_TEST_CHECK(test, reuse.ok());
    ASC_DENSE_TEST_EQ(test, reuse_report.called_provider,
                      sample.n > 0 && nrhs > 0);
    ASC_DENSE_TEST_CHECK(
        test,
        EqualBytes(sample.a.data(), factors_before.data(), sizeof(sample.a)));
    ASC_DENSE_TEST_EQ(test, sample.pivots, pivots_before);
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(b.data(), solution.data(), sizeof(b)));
    reuse_scratch.Guards(test, reuse_workspace);
  }
}

template <typename T>
void SolveCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Sample<T> sample, asc::DenseBlasLayout rhs_layout, int nrhs,
               asc::extent_t capacity, bool singular) {
  const int ldb = rhs_layout == kColumn ? sample.n + 2 : nrhs + 2;
  std::array<T, 500> b;
  b.fill(Value<T>(-317, 19));
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      Wide sum{};
      for (int k = 0; k < sample.n; ++k) {
        const auto x = Expected(k, j);
        const Wide actual_x = ToWide(Value<T>(x.real(), x.imag()));
        sum += sample.full[i * sample.n + k] * actual_x;
      }
      b[Offset(i, j, rhs_layout, ldb)] = Value<T>(sum.real(), sum.imag());
    }
  }
  const auto saved_b = b;
  auto a_view =
      Matrix(sample.a, sample.n, sample.n, sample.layout, sample.Ld());
  auto pivots = Pivots(sample.pivots, sample.n);
  auto b_view = Matrix(b, sample.n, nrhs, rhs_layout, ldb);
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, sample.triangle, sample.hermitian, a_view, pivots,
                 b_view);
  }));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, sample.n == 0 ? 0 : capacity);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return Driver(provider, sample.triangle, sample.hermitian, a_view, pivots,
                  b_view, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
  ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
  if (singular) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), sample.n);
    ASC_DENSE_TEST_CHECK(test, EqualBytes(b.data(), saved_b.data(), sizeof(b)));
  } else {
    Solution(test, sample, b, saved_b, rhs_layout, nrhs, ldb);
  }
  Reuse(test, provider, sample, b, saved_b, b_view, report, nrhs, singular);
  if (sample.n == 3 && !singular) {
    const std::size_t first = sample.triangle == kUpper ? 2 : 1;
    const asc::index_t target = sample.triangle == kUpper ? -1 : -2;
    ASC_DENSE_TEST_EQ(test, sample.pivots[first], target);
    ASC_DENSE_TEST_EQ(test, sample.pivots[first + 1], target - 1);
  }
  sample.Reconstruction(test);
  sample.Guards(test);
  // Logical RHS entries are the only writable B values, including blocked
  // solves, singular returns and empty dimensions. Preserve every gap and
  // red-zone byte in either independently selected RHS layout.
  for (std::size_t k = 0; k < b.size(); ++k) {
    const int relative = static_cast<int>(k) - 1;
    const int i = rhs_layout == kColumn ? relative % ldb : relative / ldb;
    const int j = rhs_layout == kColumn ? relative / ldb : relative % ldb;
    if (k == 0 || i >= sample.n || j >= nrhs) {
      ASC_DENSE_TEST_CHECK(test, EqualBytes(&b[k], &saved_b[k], sizeof(T)));
    }
  }
  scratch.Guards(test, workspace);
}

template <typename T>
void NanInput(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool hermitian, asc::DenseBlasTriangle triangle,
              asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 3> a{T{-3}, Value<T>(std::numeric_limits<Real>::quiet_NaN()),
                     T{-5}};
  std::array<T, 3> b{T{-7}, T{3}, T{-11}};
  std::array<asc::index_t, 3> pivots{-13, -17, -19};
  const auto old_b = b;
  auto matrix = Matrix(a, 1, 1, layout, 1);
  auto rhs = Matrix(b, 1, 1, layout, 1);
  auto pivot = Pivots(pivots, 1);
  const auto plan =
      Take(Query(provider, triangle, hermitian, matrix, pivot, rhs));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto result = WithoutAllocation(test, [&] {
    return Driver(provider, triangle, hermitian, matrix, pivot, rhs, plan,
                  workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, result.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 1);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_EQ(test, pivots[1], 1);
  ASC_DENSE_TEST_EQ(test, b, old_b);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      NanInput<T>(test, provider, hermitian, triangle, layout);
      for (const auto rhs_layout : {kColumn, kRow}) {
        for (const int n : {0, 1, 2, 3, 7, 67}) {
          const Sample<T> sample(n, hermitian, triangle, layout, 0, false);
          for (const int nrhs : {0, 1, 3}) {
            for (const asc::extent_t work :
                 {1, std::max(1, n), 64 * std::max(1, n)}) {
              SolveCase(test, provider, sample, rhs_layout, nrhs, work, false);
              ++cases;
            }
          }
        }
        for (const int n : {1, 7}) {
          const Sample<T> sample(n, hermitian, triangle, layout, 0, true);
          SolveCase(test, provider, sample, rhs_layout, 3, 1, true);
          SolveCase(test, provider, sample, rhs_layout, 3, 64 * n, true);
          cases += 2;
        }
        const int exponent = sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
        for (const int scale : {-exponent, exponent}) {
          const Sample<T> sample(7, hermitian, triangle, layout, scale, false);
          SolveCase(test, provider, sample, rhs_layout, 3, 1, false);
          SolveCase(test, provider, sample, rhs_layout, 3, 448, false);
          cases += 2;
        }
      }
    }
  }
  std::printf("rook driver cases=%d\n", cases);
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
