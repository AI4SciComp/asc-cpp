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

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_indefinite_test::Adjoint;
using asc_indefinite_test::EqualBytes;
using asc_indefinite_test::Factor;
using asc_indefinite_test::kBlock;
using asc_indefinite_test::kColumn;
using asc_indefinite_test::kHermitian;
using asc_indefinite_test::kLower;
using asc_indefinite_test::kRow;
using asc_indefinite_test::kSymmetric;
using asc_indefinite_test::kUpper;
using asc_indefinite_test::Matrix;
using asc_indefinite_test::Pivots;
using asc_indefinite_test::QueryFactor;
using asc_indefinite_test::QuerySolve;
using asc_indefinite_test::Raw;
using asc_indefinite_test::Scratch;
using asc_indefinite_test::Solve;
using asc_indefinite_test::Take;
using asc_indefinite_test::TestContext;
using asc_indefinite_test::ToWide;
using asc_indefinite_test::Value;
using asc_indefinite_test::Wide;
using asc_indefinite_test::WithoutAllocation;

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
         asc::DenseBlasLayout storage, int exponent)
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
        value *= scale;
        const Wide wide = ToWide(value);
        full[i * n + j] = wide;
        full[j * n + i] = Adjoint(wide, hermitian);
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
      const int swapped = triangle == kLower ? last - 1 : first;
      const auto raw = pivots[static_cast<std::size_t>(cursor) + 1];
      const int target = static_cast<int>(paired ? -raw : raw) - 1;
      for (int j = 0; j < n; ++j) {
        std::swap(reconstructed[swapped * n + j],
                  reconstructed[target * n + j]);
      }
      for (int i = 0; i < n; ++i) {
        std::swap(reconstructed[i * n + swapped],
                  reconstructed[i * n + target]);
      }
      cursor += triangle == kLower ? -size : size;
    }
    long double error = 0;
    long double norm = 0;
    for (int i = 0; i < n * n; ++i) {
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

template <typename T>
void CheckSolve(TestContext& test, const Sample<T>& sample,
                const std::array<T, 500>& rhs,
                const std::array<T, 500>& original, asc::DenseBlasLayout layout,
                int nrhs, int ld) {
  using Real = asc::DenseBlasRealType<T>;
  auto offset = [layout, ld](int i, int j) {
    return 1 + (layout == kColumn ? j * ld + i : i * ld + j);
  };
  auto expected = [](int i, int j) {
    return Value<T>((i % 5 - 2) / 8.0L, (j + 1) / 16.0L);
  };
  long double residual = 0;
  long double denominator = 0;
  for (int i = 0; i < sample.n; ++i) {
    for (int j = 0; j < nrhs; ++j) {
      ASC_DENSE_TEST_CHECK(
          test, std::abs(ToWide(rhs[offset(i, j)]) - ToWide(expected(i, j))) <=
                    64 * std::max(sample.n, 1) *
                        std::numeric_limits<Real>::epsilon());
      Wide product{};
      long double bound = std::abs(ToWide(original[offset(i, j)]));
      for (int k = 0; k < sample.n; ++k) {
        product += sample.full[i * sample.n + k] * ToWide(rhs[offset(k, j)]);
        bound += std::abs(sample.full[i * sample.n + k]) *
                 std::abs(ToWide(rhs[offset(k, j)]));
      }
      residual = std::max(residual,
                          std::abs(product - ToWide(original[offset(i, j)])));
      denominator = std::max(denominator, bound);
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, residual <= 32 * std::max(sample.n, 1) *
                            std::numeric_limits<Real>::epsilon() * denominator);
  for (std::size_t p = 0; p < rhs.size(); ++p) {
    const int position = static_cast<int>(p) - 1;
    const int i = layout == kColumn ? position % ld : position / ld;
    const int j = layout == kColumn ? position / ld : position % ld;
    if (p == 0 || i >= sample.n || j >= nrhs) {
      ASC_DENSE_TEST_EQ(test, rhs[p], original[p]);
    }
  }
}

template <typename T>
void SolveCases(TestContext& test, const asc::ReferenceLapackProvider& provider,
                const Sample<T>& sample,
                const asc::LapackReport& factor_report) {
  const auto factor = Take(WithoutAllocation(test, [&] {
    return asc::ReferenceBunchKaufmanFactorView<T>::Create(
        provider, sample.ConstView(), sample.triangle,
        sample.hermitian ? kHermitian : kSymmetric,
        Raw(sample.pivots, sample.n), factor_report);
  }));
  const auto saved = sample.a;
  const auto saved_pivots = sample.pivots;
  for (const auto layout : {kColumn, kRow}) {
    for (const int nrhs : {0, 1, 3}) {
      std::array<T, 500> rhs{};
      rhs.fill(Value<T>(-521, 31));
      const int ld = layout == kColumn ? sample.n + 2 : nrhs + 2;
      auto offset = [layout, ld](int i, int j) {
        return 1 + (layout == kColumn ? j * ld + i : i * ld + j);
      };
      auto expected = [](int i, int j) {
        return Value<T>((i % 5 - 2) / 8.0L, (j + 1) / 16.0L);
      };
      for (int i = 0; i < sample.n; ++i) {
        for (int j = 0; j < nrhs; ++j) {
          Wide value{};
          for (int k = 0; k < sample.n; ++k) {
            value += sample.full[i * sample.n + k] * ToWide(expected(k, j));
          }
          rhs[offset(i, j)] = Value<T>(value.real(), value.imag());
        }
      }
      const auto original = rhs;
      const auto matrix = Matrix(rhs, sample.n, nrhs, layout, ld);
      const auto plan = Take(WithoutAllocation(test, [&] {
        return QuerySolve(provider, sample.hermitian, factor, matrix);
      }));
      Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return Solve(provider, sample.hermitian, factor, matrix, plan,
                     workspace, report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      if (!status.ok()) {
        std::abort();
      }
      ASC_DENSE_TEST_EQ(test, report.called_provider,
                        sample.n != 0 && nrhs != 0);
      ASC_DENSE_TEST_EQ(test, report.native_info.has_value(),
                        report.called_provider);
      CheckSolve(test, sample, rhs, original, layout, nrhs, ld);
      scratch.Guards(test, workspace);
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(saved.data(), sample.a.data(), sizeof(saved)));
  ASC_DENSE_TEST_EQ(test, saved_pivots, sample.pivots);
}

asc::extent_t ScalarEntries(int n, bool hermitian, bool blocked, int mode) {
  if (!blocked || n == 0) {
    return 0;
  }
  if (mode == 0) {
    return 1;
  }
  if (mode == 1) {
    return -1;
  }
  if (mode == 2) {
    return (hermitian ? asc::extent_t{2} : asc::extent_t{8}) * n;
  }
  return (hermitian ? asc::extent_t{1} : asc::extent_t{7}) * n;
}

template <typename T>
void Numerics(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool hermitian) {
  constexpr int kScaleExponent =
      sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
  std::size_t count = 0;
  for (const int n : {0, 1, 2, 7, 67}) {
    for (const auto triangle : {kUpper, kLower}) {
      for (const auto layout : {kColumn, kRow}) {
        for (const bool blocked : {false, true}) {
          for (const int mode : {0, 1, 2, 3}) {
            if ((!blocked || n < 65) && mode > 1) {
              continue;
            }
            if (!blocked && mode > 0) {
              continue;
            }
            for (const int exponent : {-kScaleExponent, 0, kScaleExponent}) {
              if (n != 7 && exponent != 0) {
                continue;
              }
              Sample<T> sample(n, hermitian, triangle, layout, exponent);
              const auto matrix = sample.View();
              const auto pivots = Pivots(sample.pivots, n);
              const auto plan = Take(WithoutAllocation(test, [&] {
                return QueryFactor(provider, triangle, hermitian, blocked,
                                   matrix, pivots);
              }));
              const asc::extent_t entries =
                  ScalarEntries(n, hermitian, blocked, mode);
              Scratch<T> scratch;
              const auto workspace = scratch.Workspace(plan, entries);
              asc::LapackReport report;
              const auto status = WithoutAllocation(test, [&] {
                return Factor(provider, triangle, hermitian, blocked, matrix,
                              pivots, plan, workspace, report);
              });
              ASC_DENSE_TEST_CHECK(test, status.ok());
              if (!status.ok()) {
                std::fprintf(
                    stderr,
                    "factor failure n=%d he=%d blocked=%d "
                    "mode=%d code=%d INFO=%lld\n",
                    n, hermitian, blocked, mode,
                    static_cast<int>(status.code()),
                    static_cast<long long>(report.native_info.value_or(-999)));
                std::abort();
              }
              ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
              ASC_DENSE_TEST_EQ(test, report.factor_family, kBlock);
              sample.Reconstruction(test);
              scratch.Guards(test, workspace);
              SolveCases(test, provider, sample, report);
              ++count;
            }
          }
        }
      }
    }
  }
  std::printf(
      "%zu factor reconstructions and %zu reusable solve cases; he=%d\n", count,
      count * 6, hermitian);
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
  const std::string_view mode = argv[1];
  if (mode == "s") {
    Numerics<float>(test, provider, false);
  } else if (mode == "d") {
    Numerics<double>(test, provider, false);
  } else if (mode == "c") {
    Numerics<std::complex<float>>(test, provider, false);
  } else if (mode == "z") {
    Numerics<std::complex<double>>(test, provider, false);
  } else if (mode == "ch") {
    Numerics<std::complex<float>>(test, provider, true);
  } else if (mode == "zh") {
    Numerics<std::complex<double>>(test, provider, true);
  } else {
    return 2;
  }
  return test.Finish();
}
