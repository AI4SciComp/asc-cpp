#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_rank_revealing.h"
#include "rank_revealing_test_support.h"

namespace {
using asc::extent_t;
constexpr extent_t kRhs = 2;
using asc_rank_revealing_test::Layout;
using asc_rank_revealing_test::Matrix;
using asc_rank_revealing_test::Narrow;
using asc_rank_revealing_test::Scratch;
using asc_rank_revealing_test::Take;
using asc_rank_revealing_test::TestContext;
using asc_rank_revealing_test::Vector;
using asc_rank_revealing_test::Wide;
using asc_rank_revealing_test::Widen;
using asc_rank_revealing_test::WithoutAllocation;

template <typename T>
Wide Phase(extent_t i) {
  if constexpr (asc::DenseBlasComplex<T>) {
    constexpr std::array<Wide, 4> kPhases{Wide{1}, Wide{0, 1}, Wide{-1},
                                          Wide{0, -1}};
    return kPhases[static_cast<std::size_t>(i % 4)];
  }
  return i % 2 == 0 ? Wide{1} : Wide{-1};
}

template <typename T>
void CheckQrMath(TestContext& test, const Matrix<T>& a, extent_t m, extent_t n,
                 const std::vector<T>& tau,
                 const std::vector<asc::index_t>& pivots,
                 const std::vector<Wide>& original) {
  const extent_t k = std::min(m, n);
  std::vector<Wide> q(static_cast<std::size_t>(m * m));
  for (extent_t i = 0; i < m; ++i) {
    q[static_cast<std::size_t>(i * m + i)] = 1;
  }
  // Widened direct reflector products, independent of any ASC/provider Q API.
  for (extent_t h = 0; h < k; ++h) {
    for (extent_t row = 0; row < m; ++row) {
      Wide product = q[static_cast<std::size_t>(row * m + h)];
      for (extent_t col = h + 1; col < m; ++col) {
        product +=
            q[static_cast<std::size_t>(row * m + col)] * Widen(a(col, h));
      }
      product *= Widen(tau[static_cast<std::size_t>(h)]);
      q[static_cast<std::size_t>(row * m + h)] -= product;
      for (extent_t col = h + 1; col < m; ++col) {
        q[static_cast<std::size_t>(row * m + col)] -=
            product * std::conj(Widen(a(col, h)));
      }
    }
  }
  long double error = 0;
  long double norm = 0;
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < m; ++i) {
      Wide reconstructed{};
      for (extent_t h = 0; h < std::min(k, j + 1); ++h) {
        reconstructed +=
            q[static_cast<std::size_t>(i * m + h)] * Widen(a(h, j));
      }
      const auto wanted = original[static_cast<std::size_t>(
          i * n + pivots[static_cast<std::size_t>(j)] - 1)];
      error += std::norm(reconstructed - wanted);
      norm += std::norm(wanted);
    }
  }
  const long double tolerance =
      128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
      std::max<extent_t>(1, std::max(m, n));
  ASC_DENSE_TEST_CHECK(test, std::sqrt(error) <= tolerance * std::sqrt(norm));
  long double orthogonality = 0;
  for (extent_t i = 0; i < m; ++i) {
    for (extent_t j = 0; j < m; ++j) {
      Wide value{};
      for (extent_t h = 0; h < m; ++h) {
        value += std::conj(q[static_cast<std::size_t>(h * m + i)]) *
                 q[static_cast<std::size_t>(h * m + j)];
      }
      orthogonality += std::norm(value - Wide{i == j ? 1.0L : 0.0L});
    }
  }
  ASC_DENSE_TEST_CHECK(test, std::sqrt(orthogonality) <= tolerance);
}

template <typename T>
void CheckQr(TestContext& test, const asc::ReferenceLapackProvider& provider,
             extent_t m, extent_t n, Layout layout, bool preferred, int flags,
             bool deficient, std::string_view scalar, long double scale = 1) {
  Matrix<T> a(m, n, layout);
  std::vector<Wide> original(static_cast<std::size_t>(m * n));
  for (extent_t i = 0; i < m; ++i) {
    for (extent_t j = 0; j < n; ++j) {
      const auto column = deficient && j == n - 1 ? 0 : j;
      const auto value = (i == column ? 7.0L : 0.0L) +
                         static_cast<long double>((i + 3 * column) % 7 - 3) / 8;
      a(i, j) = Narrow<T>(scale * value * Phase<T>(i) * Phase<T>(column + 1));
      original[static_cast<std::size_t>(i * n + j)] = Widen(a(i, j));
    }
  }
  const auto before = a.bytes();
  std::vector<asc::index_t> pivots(static_cast<std::size_t>(n));
  std::vector<asc::index_t> fixed;
  for (extent_t j = 0; j < n; ++j) {
    if (flags == 2 || (flags == 1 && j % 2 == 1)) {
      pivots[static_cast<std::size_t>(j)] =
          j % 2 == 1 ? std::numeric_limits<asc::index_t>::min()
                     : std::numeric_limits<asc::index_t>::max();
      fixed.push_back(j + 1);
    }
  }
  const auto input_flags = pivots;
  std::vector<T> tau(static_cast<std::size_t>(std::min(m, n)),
                     Narrow<T>({-83, 2}));
  const auto before_tau = tau;
  const auto av = a.view();
  const auto pv = Vector(pivots, n);
  const auto tv = Vector(tau, std::min(m, n));
  asc::LapackReport report;
  const auto query = WithoutAllocation(test, [&] {
    return asc::QueryGeqp3Workspace(provider, av, pv, tv, report);
  });
  ASC_DENSE_TEST_CHECK(test, query.ok());
  if (!query.ok()) {
    return;
  }
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  a.CheckSame(test, before);
  ASC_DENSE_TEST_CHECK(test, pivots == input_flags && tau == before_tau);
  Scratch<T> scratch(*query, preferred);
  const auto workspace = scratch.view();
  const auto status = WithoutAllocation(test, [&] {
    return asc::Geqp3(provider, av, pv, tv, *query, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  if (!status.ok()) {
    return;
  }
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kColumnPivotedQr);
  for (std::size_t j = 0; j < fixed.size(); ++j) {
    ASC_DENSE_TEST_EQ(test, pivots[j], fixed[j]);
  }
  auto sorted = pivots;
  std::sort(sorted.begin(), sorted.end());
  bool permutation = true;
  for (extent_t j = 0; j < n; ++j) {
    permutation = permutation && sorted[static_cast<std::size_t>(j)] == j + 1;
  }
  ASC_DENSE_TEST_CHECK(test, permutation);
  if (permutation) {
    CheckQrMath(test, a, m, n, tau, pivots, original);
  }
  if (m == 0 && n == 6 && flags == 1) {
    ASC_DENSE_TEST_CHECK(
        test, pivots == std::vector<asc::index_t>({2, 4, 6, 1, 5, 3}));
  }
  a.CheckPadding(test, before);
  if (test.Finish() == 0) {
    std::cout << "profile scalar=" << scalar << " routine=geqp3 m=" << m
              << " n=" << n << " layout=" << static_cast<int>(layout)
              << " preferred=" << preferred << " flags=" << flags
              << " deficient=" << deficient << " scale=" << scale
              << " math=pass\n";
  }
}

struct LeastSquaresOracle {
  std::vector<Wide> original;
  std::vector<Wide> input;
  std::vector<Wide> expected;
};

template <typename T>
LeastSquaresOracle PrepareGelsy(extent_t m, extent_t n, extent_t selected_rank,
                                long double scale, Matrix<T>& a, Matrix<T>& b) {
  std::vector<Wide> original(static_cast<std::size_t>(m * n));
  std::vector<Wide> input(static_cast<std::size_t>(m * kRhs));
  std::vector<Wide> expected(static_cast<std::size_t>(n * kRhs));
  for (extent_t i = 0; i < m; ++i) {
    for (extent_t j = 0; j < n; ++j) {
      const bool nonzero =
          selected_rank != 0 && i % selected_rank == j % selected_rank;
      a(i, j) =
          Narrow<T>((nonzero ? scale : 0) * Phase<T>(i) * Phase<T>(j + 1));
      original[static_cast<std::size_t>(i * n + j)] = Widen(a(i, j));
    }
  }
  for (extent_t h = 0; h < kRhs; ++h) {
    for (extent_t j = 0; j < n; ++j) {
      expected[static_cast<std::size_t>(j * kRhs + h)] =
          selected_rank == 0
              ? Wide{}
              : std::conj(Phase<T>(j + 1)) *
                    static_cast<long double>(1 + j % selected_rank + h);
    }
    for (extent_t i = 0; i < std::max(m, n); ++i) {
      b(i, h) = Narrow<T>({std::numeric_limits<long double>::quiet_NaN(), 0});
    }
    for (extent_t i = 0; i < m; ++i) {
      Wide value{};
      for (extent_t j = 0; j < n; ++j) {
        value += original[static_cast<std::size_t>(i * n + j)] *
                 expected[static_cast<std::size_t>(j * kRhs + h)];
      }
      if (m > selected_rank && selected_rank != 0 &&
          (i == 0 || i == selected_rank)) {
        value += scale * Phase<T>(i) * (i == 0 ? 1.0L : -1.0L);
      }
      b(i, h) = Narrow<T>(value);
      input[static_cast<std::size_t>(i * kRhs + h)] = Widen(b(i, h));
    }
  }
  return {std::move(original), std::move(input), std::move(expected)};
}

template <typename T>
void CheckGelsyMath(TestContext& test, extent_t m, extent_t n,
                    extent_t selected_rank, long double scale,
                    const Matrix<T>& b, const LeastSquaresOracle& oracle) {
  using Real = asc::DenseBlasRealType<T>;
  const auto& original = oracle.original;
  const auto& input = oracle.input;
  const auto& expected = oracle.expected;
  const long double tolerance =
      256 * std::numeric_limits<Real>::epsilon() * std::max(m, n);
  for (extent_t h = 0; h < kRhs; ++h) {
    long double solution_error = 0;
    long double solution_norm = 0;
    std::vector<Wide> residual(static_cast<std::size_t>(m));
    for (extent_t j = 0; j < n; ++j) {
      const auto wanted = expected[static_cast<std::size_t>(j * kRhs + h)];
      solution_error += std::norm(Widen(b(j, h)) - wanted);
      solution_norm += std::norm(wanted);
    }
    ASC_DENSE_TEST_CHECK(
        test, std::sqrt(solution_error) <=
                  tolerance * std::max(1.0L, std::sqrt(solution_norm)));
    for (extent_t i = 0; i < m; ++i) {
      Wide value = -input[static_cast<std::size_t>(i * kRhs + h)];
      for (extent_t j = 0; j < n; ++j) {
        value += original[static_cast<std::size_t>(i * n + j)] * Widen(b(j, h));
      }
      residual[static_cast<std::size_t>(i)] = value / scale;
    }
    for (extent_t j = 0; j < n; ++j) {
      Wide normal{};
      for (extent_t i = 0; i < m; ++i) {
        normal +=
            std::conj(original[static_cast<std::size_t>(i * n + j)] / scale) *
            residual[static_cast<std::size_t>(i)];
      }
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(normal) <= tolerance * std::max(m, n));
      if (selected_rank != 0 && j >= selected_rank) {
        // Independently known nullspace vector pairs repeated columns.
        const auto projection =
            Phase<T>(j + 1) * Widen(b(j, h)) -
            Phase<T>(j % selected_rank + 1) * Widen(b(j % selected_rank, h));
        ASC_DENSE_TEST_CHECK(test, std::abs(projection) <= tolerance);
      }
    }
  }
}

template <typename T>
void CheckGelsy(TestContext& test, const asc::ReferenceLapackProvider& provider,
                extent_t m, extent_t n, extent_t selected_rank, Layout al,
                Layout bl, bool preferred, bool fixed_first, long double scale,
                std::string_view scalar) {
  Matrix<T> a(m, n, al);
  Matrix<T> b(std::max(m, n), kRhs, bl);
  const auto oracle = PrepareGelsy(m, n, selected_rank, scale, a, b);
  const auto before_a = a.bytes();
  const auto before_b = b.bytes();
  std::vector<asc::index_t> pivots(static_cast<std::size_t>(n));
  if (fixed_first && n != 0) {
    pivots[0] = std::numeric_limits<asc::index_t>::min();
  }
  const auto input_flags = pivots;
  const auto av = a.view();
  const auto bv = b.view();
  const auto pv = Vector(pivots, n);
  using Real = asc::DenseBlasRealType<T>;
  const Real rcond = 32 * std::numeric_limits<Real>::epsilon();
  asc::LapackReport report;
  const auto query = WithoutAllocation(test, [&] {
    return asc::QueryGelsyWorkspace(provider, av, bv, pv, rcond, report);
  });
  ASC_DENSE_TEST_CHECK(test, query.ok());
  if (!query.ok()) {
    return;
  }
  a.CheckSame(test, before_a);
  b.CheckSame(test, before_b);
  ASC_DENSE_TEST_CHECK(test, pivots == input_flags);
  Scratch<T> scratch(*query, preferred);
  const auto workspace = scratch.view();
  asc::index_t rank = -7;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Gelsy(provider, av, bv, pv, rcond, rank, *query, workspace,
                      report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  if (!status.ok()) {
    return;
  }
  ASC_DENSE_TEST_EQ(test, rank, selected_rank);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kRankDecision);
  if (selected_rank == 0) {
    ASC_DENSE_TEST_CHECK(test, pivots == input_flags);
  }
  CheckGelsyMath(test, m, n, selected_rank, scale, b, oracle);
  for (extent_t i = n; i < m; ++i) {
    for (extent_t h = 0; h < kRhs; ++h) {
      ASC_DENSE_TEST_CHECK(test, asc_rank_revealing_test::SameBits(
                                     b(i, h), before_b[b.Offset(i, h)]));
    }
  }
  a.CheckPadding(test, before_a);
  b.CheckPadding(test, before_b);
  if (test.Finish() == 0) {
    std::cout << "profile scalar=" << scalar << " routine=gelsy m=" << m
              << " n=" << n << " rank=" << rank
              << " a_layout=" << static_cast<int>(al)
              << " b_layout=" << static_cast<int>(bl)
              << " preferred=" << preferred << " fixed=" << fixed_first
              << " scale=" << scale << " solution_optimality_nullspace=pass\n";
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         std::string_view scalar, std::string_view cold) {
  if (cold == "geqp3") {
    CheckQr<T>(test, provider, 137, 130, Layout::kRowMajor, true, 0, false,
               scalar);
    return;
  }
  if (cold == "gelsy") {
    CheckGelsy<T>(test, provider, 4, 3, 2, Layout::kRowMajor,
                  Layout::kColumnMajor, false, false, 1, scalar);
    return;
  }
  for (const auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
    for (const bool preferred : {false, true}) {
      for (const auto shape : {std::array<extent_t, 2>{0, 6},
                               {5, 0},
                               {0, 0},
                               {1, 1},
                               {4, 3},
                               {3, 5},
                               {137, 130}}) {
        for (const int flags : {0, 1, 2}) {
          CheckQr<T>(test, provider, shape[0], shape[1], layout, preferred,
                     flags, false, scalar);
        }
      }
      CheckQr<T>(test, provider, 4, 3, layout, preferred, 0, true, scalar);
      for (const auto other : {Layout::kColumnMajor, Layout::kRowMajor}) {
        for (const auto shape : {std::array<extent_t, 3>{4, 2, 2},
                                 {4, 3, 2},
                                 {2, 4, 2},
                                 {3, 3, 0},
                                 {1, 1, 1}}) {
          for (const bool fixed : {false, true}) {
            CheckGelsy<T>(test, provider, shape[0], shape[1], shape[2], layout,
                          other, preferred, fixed, 1, scalar);
          }
        }
      }
    }
  }
  using Real = asc::DenseBlasRealType<T>;
  for (const auto scale :
       {static_cast<long double>(std::numeric_limits<Real>::min()) / 8,
        static_cast<long double>(std::numeric_limits<Real>::max()) / 64}) {
    for (const auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
      for (const bool preferred : {false, true}) {
        CheckQr<T>(test, provider, 4, 3, layout, preferred, 0, false, scalar,
                   scale);
      }
    }
    CheckGelsy<T>(test, provider, 4, 3, 2, Layout::kRowMajor,
                  Layout::kColumnMajor, false, false, scale, scalar);
  }
}
}  // namespace

int main(int argc, char** argv) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (argc != 2 && argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view cold = argc == 3 ? argv[2] : "";
  if (!cold.empty() && cold != "geqp3" && cold != "gelsy") {
    return 2;
  }
  if (scalar == "s") {
    Run<float>(test, provider, scalar, cold);
  } else if (scalar == "d") {
    Run<double>(test, provider, scalar, cold);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider, scalar, cold);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, scalar, cold);
  } else {
    return 2;
  }
  return test.Finish();
}
