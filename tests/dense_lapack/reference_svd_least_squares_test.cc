#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "least_squares_test_support.h"
#include "svd_least_squares_faults.h"
#include "svd_least_squares_test_support.h"

namespace {
using asc_svd_least_squares_test::Execute;
using asc_svd_least_squares_test::ForeignCalls;
using asc_svd_least_squares_test::Guarded;
using asc_svd_least_squares_test::Layout;
using asc_svd_least_squares_test::Matrix;
using asc_svd_least_squares_test::Narrow;
using asc_svd_least_squares_test::NotANumber;
using asc_svd_least_squares_test::Query;
using asc_svd_least_squares_test::QueryCalls;
using asc_svd_least_squares_test::Routine;
using asc_svd_least_squares_test::SameBits;
using asc_svd_least_squares_test::Scratch;
using asc_svd_least_squares_test::Take;
using asc_svd_least_squares_test::TestContext;
using asc_svd_least_squares_test::Wide;
using asc_svd_least_squares_test::Widen;
using asc_svd_least_squares_test::WithoutAllocation;

// Independent exact unitary factors: a rational Householder matrix with unit
// row phases. No ASC/provider factorization constructs the expected answer.
template <typename T>
Wide Basis(asc::extent_t size, asc::extent_t row, asc::extent_t column) {
  Wide phase{1, 0};
  if constexpr (asc::DenseBlasComplex<T>) {
    constexpr std::array<Wide, 4> kPhases{Wide{1, 0}, Wide{0, 1}, Wide{-1, 0},
                                          Wide{0, -1}};
    phase = kPhases[static_cast<std::size_t>(row % 4)];
  }
  return phase * ((row == column ? 1.L : 0.L) - 2.L / size);
}

template <typename T>
struct KnownProblem {
  using Real = asc::DenseBlasRealType<T>;
  Routine routine;
  asc::extent_t m;
  asc::extent_t n;
  asc::extent_t nrhs;
  asc::extent_t expected_rank;
  Layout a_layout;
  Layout b_layout;
  bool preferred;
  long double scale;
  std::string_view scalar;
  TestContext test;
  asc::extent_t k;
  asc::extent_t capacity;
  Matrix<T> a;
  Matrix<T> b;
  Guarded<Real> s;
  std::vector<Wide> original_a;
  std::vector<Wide> original_b;
  std::vector<Wide> solution;
  KnownProblem(Routine operation, asc::extent_t rows, asc::extent_t columns,
               asc::extent_t rhs_count, asc::extent_t wanted_rank,
               Layout matrix_layout, Layout rhs_layout, bool use_preferred,
               long double input_scale, std::string_view scalar_name)
      : routine(operation),
        m(rows),
        n(columns),
        nrhs(rhs_count),
        expected_rank(wanted_rank),
        a_layout(matrix_layout),
        b_layout(rhs_layout),
        preferred(use_preferred),
        scale(input_scale),
        scalar(scalar_name),
        k(std::min(m, n)),
        capacity(std::max(m, n)),
        a(m, n, a_layout),
        b(capacity, nrhs, b_layout),
        s(static_cast<std::size_t>(k)),
        original_a(static_cast<std::size_t>(m * n)),
        original_b(static_cast<std::size_t>(m * nrhs)),
        solution(static_cast<std::size_t>(n * nrhs)) {
    InitializeMatrix();
    InitializeRightHandSides();
  }
  void InitializeMatrix() {
    for (asc::extent_t i = 0; i < m; ++i) {
      for (asc::extent_t j = 0; j < n; ++j) {
        Wide value{};
        for (asc::extent_t l = 0; l < expected_rank; ++l) {
          value += Basis<T>(m, i, l) *
                   static_cast<long double>(expected_rank - l) *
                   std::conj(Basis<T>(n, j, l));
        }
        a(i, j) = Narrow<T>(value * scale);
        original_a[static_cast<std::size_t>(i * n + j)] = Widen(a(i, j));
      }
    }
  }
  void InitializeRightHandSides() {
    for (asc::extent_t j = 0; j < nrhs; ++j) {
      for (asc::extent_t i = 0; i < n; ++i) {
        Wide value{};
        for (asc::extent_t l = 0; l < expected_rank; ++l) {
          const long double coefficient = (l % 5 - 2) * (j + 1.L) / 3.L;
          value += Basis<T>(n, i, l) * coefficient;
        }
        solution[static_cast<std::size_t>(j * n + i)] = value;
      }
      for (asc::extent_t i = 0; i < capacity; ++i) {
        if (i >= m) {
          // Output-only wide B slots must not be numerical inputs.
          b(i, j) = NotANumber<T>();
          continue;
        }
        Wide value{};
        for (asc::extent_t l = 0; l < expected_rank; ++l) {
          const long double coefficient = (l % 5 - 2) * (j + 1.L) / 3.L;
          value += Basis<T>(m, i, l) *
                   static_cast<long double>(expected_rank - l) * coefficient;
        }
        if (expected_rank < m) {
          value += Basis<T>(m, i, expected_rank) * (j + 1.L) / 3.L;
        }
        b(i, j) = Narrow<T>(value * scale);
        original_b[static_cast<std::size_t>(j * m + i)] = Widen(b(i, j));
      }
    }
    if ((a_layout == Layout::kRowMajor ? m : n) > 1) {
      a.view().data()[a_layout == Layout::kRowMajor ? n : m] = NotANumber<T>();
    }
    if ((b_layout == Layout::kRowMajor ? capacity : nrhs) > 1) {
      b.view().data()[b_layout == Layout::kRowMajor ? nrhs : capacity] =
          NotANumber<T>();
    }
  }
  void CheckSingularValues(long double tolerance) {
    for (asc::extent_t i = 0; i < k; ++i) {
      const long double expected =
          std::max<asc::extent_t>(0, expected_rank - i);
      ASC_DENSE_TEST_NEAR(test, static_cast<long double>(s.data()[i]) / scale,
                          expected, tolerance, tolerance);
      ASC_DENSE_TEST_CHECK(test, s.data()[i] >= 0);
      if (i > 0) {
        ASC_DENSE_TEST_CHECK(test, s.data()[i - 1] >= s.data()[i]);
      }
    }
  }
  void CheckSolutions(const std::vector<T>& b_before, long double tolerance) {
    for (asc::extent_t j = 0; j < nrhs; ++j) {
      std::vector<Wide> residual(static_cast<std::size_t>(m));
      long double residual_squared = 0;
      for (asc::extent_t i = 0; i < n; ++i) {
        ASC_DENSE_TEST_CHECK(
            test, std::abs(Widen(b(i, j)) -
                           solution[static_cast<std::size_t>(j * n + i)]) <=
                      tolerance * (j + 1));
      }
      for (asc::extent_t i = 0; i < m; ++i) {
        Wide value = -original_b[static_cast<std::size_t>(j * m + i)] / scale;
        for (asc::extent_t l = 0; l < n; ++l) {
          value += original_a[static_cast<std::size_t>(i * n + l)] / scale *
                   Widen(b(l, j));
        }
        residual[static_cast<std::size_t>(i)] = value;
        residual_squared += std::norm(value);
      }
      const long double expected_squared =
          expected_rank < m ? (j + 1.L) * (j + 1.L) / 9.L : 0;
      ASC_DENSE_TEST_NEAR(test, residual_squared, expected_squared,
                          tolerance * (j + 1), tolerance);
      // Least-squares optimality uses the ORIGINAL conjugate transpose, not
      // the provider's transformed B or the expected solution as an oracle.
      for (asc::extent_t l = 0; l < n; ++l) {
        Wide gradient{};
        for (asc::extent_t i = 0; i < m; ++i) {
          gradient +=
              std::conj(original_a[static_cast<std::size_t>(i * n + l)] /
                        scale) *
              residual[static_cast<std::size_t>(i)];
        }
        ASC_DENSE_TEST_CHECK(
            test, std::abs(gradient) <= tolerance * capacity * (j + 1));
      }
      // Every omitted right singular direction is an independently known
      // null vector. Orthogonality to all of them proves the minimum norm.
      for (asc::extent_t l = expected_rank; l < n; ++l) {
        Wide component{};
        for (asc::extent_t i = 0; i < n; ++i) {
          component += std::conj(Basis<T>(n, i, l)) * Widen(b(i, j));
        }
        ASC_DENSE_TEST_CHECK(test, std::abs(component) <= tolerance * (j + 1));
      }
      if (m > n && expected_rank < n) {
        for (asc::extent_t i = n; i < m; ++i) {
          ASC_DENSE_TEST_CHECK(test,
                               SameBits(b(i, j), b_before[b.Offset(i, j)]));
        }
      }
      if (m >= n && expected_rank == n && scale == 1) {
        long double transformed_squared = 0;
        for (asc::extent_t i = n; i < m; ++i) {
          transformed_squared += std::norm(Widen(b(i, j)));
        }
        ASC_DENSE_TEST_NEAR(test, transformed_squared, expected_squared,
                            tolerance * (j + 1), tolerance);
      }
    }
  }
  [[nodiscard]] bool Record(bool passed) const {
    if (passed) {
      std::cout
          << "SVD_LS_PROFILE scalar=" << scalar
          << " routine=" << (routine == Routine::kGelss ? "gelss" : "gelsd")
          << " m=" << m << " n=" << n << " nrhs=" << nrhs
          << " rank=" << expected_rank
          << " a_layout=" << static_cast<int>(a_layout)
          << " b_layout=" << static_cast<int>(b_layout)
          << " preferred=" << preferred << " scale=" << scale
          << " evidence=singular_values,solution,optimality,nullspace,padding"
          << '\n';
    } else {
      std::cerr << "FAILED scalar=" << scalar
                << " routine=" << static_cast<int>(routine) << " m=" << m
                << " n=" << n << " rank=" << expected_rank << " scale=" << scale
                << '\n';
    }
    return passed;
  }
  bool ExecuteKnown(const asc::ReferenceLapackProvider& provider) {
    const auto a_before = a.bytes();
    const auto b_before = b.bytes();
    const auto s_before = s.bytes();
    const Real rcond = expected_rank == k ? Real{-1} : Real{0.001};
    asc::LapackReport report;
    const auto query_calls = QueryCalls();
    const auto foreign_calls = ForeignCalls();
    const auto query = WithoutAllocation(test, [&] {
      return Query(provider, routine, a.view(), b.view(), s.vector(), rcond,
                   report);
    });
    ASC_DENSE_TEST_CHECK(test, query.ok());
    if (!query.ok()) {
      return false;
    }
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, QueryCalls(), query_calls + 1);
    ASC_DENSE_TEST_EQ(test, ForeignCalls(), foreign_calls + 1);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
    a.CheckSame(test, a_before);
    b.CheckSame(test, b_before);
    s.CheckSame(test, s_before);
    Scratch<T> scratch(*query, preferred);
    const auto workspace = scratch.view();
    asc::index_t rank = -79;
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, a.view(), b.view(), s.vector(), rcond,
                     rank, *query, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, ForeignCalls(), foreign_calls + 2);
    ASC_DENSE_TEST_EQ(test, QueryCalls(), query_calls + 1);
    ASC_DENSE_TEST_EQ(test, rank, expected_rank);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kRankDecision);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
    const long double tolerance =
        128.L * std::numeric_limits<Real>::epsilon() * capacity;
    CheckSingularValues(tolerance);
    CheckSolutions(b_before, tolerance);
    a.CheckPadding(test, a_before);
    b.CheckPadding(test, b_before);
    s.CheckGuards(test);
    scratch.CheckGuards(test);
    return Record(test.Finish() == 0);
  }
};

template <typename T>
bool SolveKnown(const asc::ReferenceLapackProvider& provider, Routine routine,
                asc::extent_t m, asc::extent_t n, asc::extent_t nrhs,
                asc::extent_t expected_rank, Layout a_layout, Layout b_layout,
                bool preferred, long double scale, std::string_view scalar) {
  KnownProblem<T> problem(routine, m, n, nrhs, expected_rank, a_layout,
                          b_layout, preferred, scale, scalar);
  return problem.ExecuteKnown(provider);
}

template <typename T>
int Run(const asc::ReferenceLapackProvider& provider, std::string_view scalar,
        std::string_view cold) {
  using Real = asc::DenseBlasRealType<T>;
  bool passed = true;
  constexpr std::array kLayouts{Layout::kRowMajor, Layout::kColumnMajor};
  for (const Routine routine : {Routine::kGelss, Routine::kGelsd}) {
    if ((!cold.empty() && cold == "gelss" && routine != Routine::kGelss) ||
        (!cold.empty() && cold == "gelsd" && routine != Routine::kGelsd)) {
      continue;
    }
    if (!cold.empty()) {
      passed &=
          SolveKnown<T>(provider, routine, 28, 26, 3, 25, Layout::kRowMajor,
                        Layout::kColumnMajor, true, 1, scalar);
      continue;
    }
    for (const auto shape : {std::array<asc::extent_t, 2>{5, 3},
                             {3, 5},
                             {3, 3},
                             {1, 1},
                             {7, 4},
                             {4, 7},
                             {28, 26},
                             {26, 28},
                             {53, 53}}) {
      const auto k = std::min(shape[0], shape[1]);
      for (const auto rank : {k, k - 1, asc::extent_t{0}}) {
        for (const auto a_layout : kLayouts) {
          for (const auto b_layout : kLayouts) {
            for (const bool preferred : {false, true}) {
              passed &=
                  SolveKnown<T>(provider, routine, shape[0], shape[1], 3, rank,
                                a_layout, b_layout, preferred, 1, scalar);
            }
          }
        }
      }
    }
    for (const auto scale :
         {static_cast<long double>(std::numeric_limits<Real>::min()) * 128,
          static_cast<long double>(std::numeric_limits<Real>::max()) / 65536}) {
      for (const auto a_layout : kLayouts) {
        for (const bool preferred : {false, true}) {
          passed &=
              SolveKnown<T>(provider, routine, 5, 3, 1, 3, a_layout,
                            Layout::kColumnMajor, preferred, scale, scalar);
          passed &= SolveKnown<T>(provider, routine, 3, 5, 1, 2, a_layout,
                                  Layout::kRowMajor, preferred, scale, scalar);
        }
      }
    }
  }
  for (const Routine routine : {Routine::kGelss, Routine::kGelsd}) {
    if (!cold.empty()) {
      break;
    }
    for (const auto a_layout : kLayouts) {
      for (const bool preferred : {false, true}) {
        passed &= SolveKnown<T>(provider, routine, 1, 1000, 1, 1, a_layout,
                                Layout::kColumnMajor, preferred, 1, scalar);
      }
    }
  }
  return passed ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc < 2 || argc > 3) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  const std::string_view cold = argc == 3 ? argv[2] : "";
  if (scalar == "s") {
    return Run<float>(provider, scalar, cold);
  }
  if (scalar == "d") {
    return Run<double>(provider, scalar, cold);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(provider, scalar, cold);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(provider, scalar, cold);
  }
  return 2;
}
