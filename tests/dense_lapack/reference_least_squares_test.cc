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
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "least_squares_faults.h"
#include "least_squares_test_support.h"

namespace {
using asc::extent_t;
using asc_least_squares_test::Execute;
using asc_least_squares_test::Fault;
using asc_least_squares_test::ForeignCalls;
using asc_least_squares_test::kPacking;
using asc_least_squares_test::kScalar;
using asc_least_squares_test::Layout;
using asc_least_squares_test::Matrix;
using asc_least_squares_test::MinimumQueries;
using asc_least_squares_test::Narrow;
using asc_least_squares_test::NotANumber;
using asc_least_squares_test::Query;
using asc_least_squares_test::QueryCalls;
using asc_least_squares_test::Routine;
using asc_least_squares_test::RoutineCalls;
using asc_least_squares_test::SameBits;
using asc_least_squares_test::Scratch;
using asc_least_squares_test::SetFault;
using asc_least_squares_test::Take;
using asc_least_squares_test::TestContext;
using asc_least_squares_test::Wide;
using asc_least_squares_test::Widen;
using asc_least_squares_test::WithoutAllocation;

template <typename T>
asc::DenseBlasTranspose Transpose(bool adjoint) {
  if (!adjoint) {
    return asc::DenseBlasTranspose::kNone;
  }
  return asc::DenseBlasComplex<T> ? asc::DenseBlasTranspose::kConjugateTranspose
                                  : asc::DenseBlasTranspose::kTranspose;
}

template <typename T>
Wide Phase(extent_t i) {
  if constexpr (asc::DenseBlasComplex<T>) {
    constexpr std::array<Wide, 4> kPhases{Wide{1}, Wide{0, 1}, Wide{-1},
                                          Wide{0, -1}};
    return kPhases[static_cast<std::size_t>(i % 4)];
  }
  return i % 2 == 0 ? Wide{1} : Wide{-1};
}

struct Problem {
  extent_t equations;
  extent_t unknowns;
  extent_t rhs;
  bool runbook;
};

// The oracle is built from independently chosen solutions and orthogonal
// residual/nullspace vectors. No ASC solver, factorization or normal-equation
// solve is used to construct expectations.
struct Oracle {
  std::vector<Wide> effective;
  std::vector<Wide> expected;
  std::vector<Wide> original_rhs;
};

template <typename T>
Oracle PrepareProblem(Problem problem, bool adjoint, long double scale,
                      Matrix<T>& a, Matrix<T>& b) {
  const auto e = problem.equations;
  const auto u = problem.unknowns;
  const auto nrhs = problem.rhs;
  const auto capacity = std::max(e, u);
  std::vector<Wide> effective(static_cast<std::size_t>(e * u));
  std::vector<Wide> expected(static_cast<std::size_t>(u * nrhs));
  std::vector<Wide> original_rhs(static_cast<std::size_t>(e * nrhs));
  for (extent_t i = 0; i < e; ++i) {
    for (extent_t j = 0; j < u; ++j) {
      long double value = 0;
      if (problem.runbook && e == 3 && u == 2) {
        value = (i == j || i == 2) ? 1 : 0;
      } else if (e != 0 && u != 0) {
        const bool nonzero = e >= u ? i % u == j : j % e == i;
        value = nonzero ? 1 : 0;
      }
      const Wide entry = scale * Phase<T>(i) * value * Phase<T>(j + 1);
      effective[static_cast<std::size_t>(i * u + j)] = entry;
      if (adjoint) {
        a(j, i) = Narrow<T>(std::conj(entry));
      } else {
        a(i, j) = Narrow<T>(entry);
      }
    }
  }
  for (extent_t h = 0; h < nrhs; ++h) {
    for (extent_t j = 0; j < u; ++j) {
      long double value = 0;
      if (e != 0) {
        value = static_cast<long double>((e >= u ? j : j % e) + 1 + h);
      }
      if (problem.runbook && e == 3 && u == 2) {
        value = (j == 0 ? 2 : -1) * static_cast<long double>(h + 1);
      }
      expected[static_cast<std::size_t>(j * nrhs + h)] =
          std::conj(Phase<T>(j + 1)) * value;
    }
    for (extent_t i = 0; i < capacity; ++i) {
      b(i, h) = NotANumber<T>();
    }
    for (extent_t i = 0; i < e; ++i) {
      Wide value{};
      for (extent_t j = 0; j < u; ++j) {
        value += effective[static_cast<std::size_t>(i * u + j)] *
                 expected[static_cast<std::size_t>(j * nrhs + h)];
      }
      if (problem.runbook && e == 3 && u == 2) {
        value += scale * Phase<T>(i) * (i == 2 ? -1.0L : 1.0L) *
                 static_cast<long double>(h + 1);
      } else if (e > u && u != 0) {
        if (i == 0 || i == u) {
          value += scale * Phase<T>(i) * (i == 0 ? 1.0L : -1.0L);
        }
      }
      b(i, h) = Narrow<T>(value);
      original_rhs[static_cast<std::size_t>(i * nrhs + h)] = Widen(b(i, h));
    }
  }

  return {std::move(effective), std::move(expected), std::move(original_rhs)};
}

template <typename T>
void CheckResidualCoordinates(TestContext& test, Problem problem,
                              long double scale, const Matrix<T>& b,
                              const Oracle& oracle, Routine routine,
                              extent_t rhs, long double tolerance) {
  if (routine == Routine::kGetsls || problem.equations <= problem.unknowns ||
      problem.unknowns == 0 || scale != 1) {
    return;
  }
  // With unscaled inputs, the orthogonal transform preserves the residual
  // norm. The coordinates need not equal the original residual vector.
  long double coordinates_squared = 0;
  long double residual_squared = 0;
  for (extent_t i = problem.unknowns; i < problem.equations; ++i) {
    coordinates_squared += std::norm(Widen(b(i, rhs)));
  }
  for (extent_t i = 0; i < problem.equations; ++i) {
    Wide residual =
        -oracle.original_rhs[static_cast<std::size_t>(i * problem.rhs + rhs)];
    for (extent_t j = 0; j < problem.unknowns; ++j) {
      residual +=
          oracle.effective[static_cast<std::size_t>(i * problem.unknowns + j)] *
          Widen(b(j, rhs));
    }
    residual_squared += std::norm(residual);
  }
  ASC_DENSE_TEST_CHECK(test, std::abs(coordinates_squared - residual_squared) <=
                                 tolerance * std::max(1.0L, residual_squared));
}

template <typename T>
void CheckMinimumNorm(TestContext& test, Problem problem, const Matrix<T>& b,
                      extent_t rhs, long double tolerance) {
  const auto e = problem.equations;
  const auto u = problem.unknowns;
  if (u > e && e != 0) {
    for (extent_t j = e; j < u; ++j) {
      // Equal phase-adjusted duplicate columns have a null vector supported
      // on j and j%e. Orthogonality gives the unique minimum-norm solution.
      const auto dot = Phase<T>(j + 1) * Widen(b(j, rhs)) -
                       Phase<T>(j % e + 1) * Widen(b(j % e, rhs));
      ASC_DENSE_TEST_CHECK(test, std::abs(dot) <= tolerance * 10);
    }
  }
}

template <typename T>
void CheckMathematics(TestContext& test, Problem problem, long double scale,
                      const Matrix<T>& b, const Oracle& oracle, Routine routine,
                      const std::vector<T>& old_b) {
  const auto e = problem.equations;
  const auto u = problem.unknowns;
  const auto nrhs = problem.rhs;
  const auto capacity = std::max(e, u);
  const auto& effective = oracle.effective;
  const auto& expected = oracle.expected;
  const auto& original_rhs = oracle.original_rhs;
  const long double tolerance =
      80 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
      static_cast<long double>(std::max<extent_t>(1, std::max(e, u)));
  for (extent_t h = 0; h < nrhs; ++h) {
    for (extent_t j = 0; j < u; ++j) {
      const Wide value = Widen(b(j, h));
      const Wide exact = expected[static_cast<std::size_t>(j * nrhs + h)];
      ASC_DENSE_TEST_CHECK(test, std::isfinite(std::abs(value)));
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(value - exact) <=
                               tolerance * std::max(1.0L, std::abs(exact)));
    }
    // Optimality is independently checked on original, unscaled A/B. The
    // verifier divides by the known nonzero scaling before products, avoiding
    // overflow and underflow even for near-extreme input magnitudes.
    std::vector<Wide> residual(static_cast<std::size_t>(e));
    for (extent_t i = 0; i < e; ++i) {
      residual[static_cast<std::size_t>(i)] =
          -original_rhs[static_cast<std::size_t>(i * nrhs + h)] / scale;
      for (extent_t j = 0; j < u; ++j) {
        residual[static_cast<std::size_t>(i)] +=
            effective[static_cast<std::size_t>(i * u + j)] / scale *
            Widen(b(j, h));
      }
    }
    for (extent_t j = 0; j < u; ++j) {
      Wide dot{};
      long double denominator = 0;
      for (extent_t i = 0; i < e; ++i) {
        const auto value =
            effective[static_cast<std::size_t>(i * u + j)] / scale;
        dot += std::conj(value) * residual[static_cast<std::size_t>(i)];
        denominator +=
            std::abs(value) *
            (std::abs(original_rhs[static_cast<std::size_t>(i * nrhs + h)] /
                      scale) +
             std::abs(residual[static_cast<std::size_t>(i)]));
      }
      ASC_DENSE_TEST_CHECK(
          test, denominator == 0 ? std::abs(dot) == 0
                                 : std::abs(dot) / denominator <= tolerance);
    }
    CheckMinimumNorm(test, problem, b, h, tolerance);
    CheckResidualCoordinates(test, problem, scale, b, oracle, routine, h,
                             tolerance);
    if (routine == Routine::kGetsls) {
      for (extent_t i = u; i < capacity; ++i) {
        ASC_DENSE_TEST_CHECK(test, SameBits(b(i, h), old_b[b.Offset(i, h)]));
      }
    }
  }
}

template <typename T>
void CheckProblem(TestContext& test,
                  const asc::ReferenceLapackProvider& provider, Routine routine,
                  Problem problem, bool adjoint, Layout a_layout,
                  Layout b_layout, bool preferred, long double scale,
                  std::string_view scalar) {
  const auto e = problem.equations;
  const auto u = problem.unknowns;
  const auto nrhs = problem.rhs;
  const auto m = adjoint ? u : e;
  const auto n = adjoint ? e : u;
  const auto capacity = std::max(e, u);
  Matrix<T> a(m, n, a_layout);
  Matrix<T> b(capacity, nrhs, b_layout);
  const auto oracle = PrepareProblem(problem, adjoint, scale, a, b);
  const auto old_a = a.bytes();
  const auto old_b = b.bytes();
  asc::LapackReport report;
  const auto before_queries = QueryCalls();
  const auto before_minimum = MinimumQueries();
  const auto before_routine = RoutineCalls(routine);
  const auto result = WithoutAllocation(test, [&] {
    return Query(provider, routine, Transpose<T>(adjoint), a.view(), b.view(),
                 report);
  });
  ASC_DENSE_TEST_CHECK(test, result.ok());
  if (!result.ok()) {
    return;
  }
  const auto& plan = *result;
  const std::size_t query_count = routine == Routine::kGetsls ? 2 : 1;
  ASC_DENSE_TEST_EQ(test, QueryCalls() - before_queries, query_count);
  ASC_DENSE_TEST_EQ(test, MinimumQueries() - before_minimum,
                    routine == Routine::kGetsls ? 1U : 0U);
  ASC_DENSE_TEST_EQ(test, RoutineCalls(routine) - before_routine, query_count);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  a.CheckSame(test, old_a);
  b.CheckSame(test, old_b);
  Scratch<T> scratch(plan, preferred);
  const auto workspace = scratch.view();
  const auto calls = ForeignCalls();
  const auto queries = QueryCalls();
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, Transpose<T>(adjoint), a.view(), b.view(),
                   plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, QueryCalls(), queries);
  const bool empty = std::min({m, n, nrhs}) == 0;
  ASC_DENSE_TEST_EQ(test, ForeignCalls() - calls, empty ? 0U : 1U);
  ASC_DENSE_TEST_EQ(test, report.called_provider, !empty);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), !empty);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  if (!status.ok()) {
    return;
  }
  CheckMathematics(test, problem, scale, b, oracle, routine, old_b);
  a.CheckPadding(test, old_a);
  b.CheckPadding(test, old_b);
  if (test.Finish() != 0) {
    return;
  }
  std::cout << R"({"profile":"reference-full-rank-least-squares","scalar":")"
            << scalar << R"(","routine":)" << static_cast<int>(routine)
            << ",\"m\":" << m << ",\"n\":" << n << ",\"nrhs\":" << nrhs
            << ",\"adjoint\":" << adjoint
            << ",\"a_layout\":" << static_cast<int>(a_layout)
            << ",\"b_layout\":" << static_cast<int>(b_layout)
            << ",\"preferred\":" << preferred << ",\"scaled\":" << (scale != 1)
            << "}\n";
}

template <typename T>
void NumericalFailure(TestContext& test,
                      const asc::ReferenceLapackProvider& provider,
                      Routine routine, bool adjoint, Layout layout) {
  Matrix<T> a(2, 2, layout);
  Matrix<T> b(2, 2, layout);
  for (extent_t i = 0; i < 2; ++i) {
    for (extent_t j = 0; j < 2; ++j) {
      a(i, j) = i == 0 && j == 0 ? T{1} : T{};
      b(i, j) = static_cast<T>(i + j + 1);
    }
  }
  asc::LapackReport report;
  const auto plan = Take(Query(provider, routine, Transpose<T>(adjoint),
                               a.view(), b.view(), report));
  Scratch<T> scratch(plan, false);
  const auto workspace = scratch.view();
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, Transpose<T>(adjoint), a.view(), b.view(),
                   plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info, 2);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_EQ(test, a(0, 0), T{1});
  ASC_DENSE_TEST_EQ(test, a(1, 1), T{});
  for (extent_t i = 0; i < 2; ++i) {
    for (extent_t j = 0; j < 2; ++j) {
      ASC_DENSE_TEST_EQ(test, b(i, j), static_cast<T>(i + j + 1));
      a(i, j) = T{};
    }
  }
  const auto zero = WithoutAllocation(test, [&] {
    return Execute(provider, routine, Transpose<T>(adjoint), a.view(), b.view(),
                   plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, zero.ok());
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  for (extent_t i = 0; i < 2; ++i) {
    for (extent_t j = 0; j < 2; ++j) {
      ASC_DENSE_TEST_EQ(test, b(i, j), T{});
    }
  }
}

template <typename T>
void Failures(TestContext& test, const asc::ReferenceLapackProvider& provider,
              Routine routine) {
  Matrix<T> a(3, 2, Layout::kRowMajor);
  Matrix<T> b(3, 2, Layout::kColumnMajor);
  asc::LapackReport report;
  const auto trans = Transpose<T>(false);
  const auto plan =
      Take(Query(provider, routine, trans, a.view(), b.view(), report));
  Scratch<T> scratch(plan, true);
  const auto workspace = scratch.view();
  const auto before_a = a.bytes();
  const auto before_b = b.bytes();
  const auto calls = ForeignCalls();
  const auto rejected = [&](auto operation) {
    const auto status = WithoutAllocation(test, operation);
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    a.CheckSame(test, before_a);
    b.CheckSame(test, before_b);
  };
  rejected([&] {
    // Fixed uint8_t underlying enum permits 99; it is intentionally
    // unsupported. NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return Execute(provider, routine, static_cast<asc::DenseBlasTranspose>(99),
                   a.view(), b.view(), plan, workspace, report);
  });
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    rejected([&] {
      return Execute(provider, routine, trans, a.view(space), b.view(), plan,
                     workspace, report);
    });
    for (const auto role : {kScalar, kPacking}) {
      auto bad = workspace;
      const auto region = bad.regions[role];
      bad.regions[role] = {region.data(), region.size(), space};
      rejected([&] {
        return Execute(provider, routine, trans, a.view(), b.view(), plan, bad,
                       report);
      });
    }
  }
  for (const auto role : {kScalar, kPacking}) {
    auto bad = workspace;
    const auto region = bad.regions[role];
    const auto bytes =
        static_cast<std::size_t>(plan.regions[role].minimum_entries) *
        sizeof(T);
    bad.regions[role] = {region.data(), bytes - 1, region.space()};
    rejected([&] {
      return Execute(provider, routine, trans, a.view(), b.view(), plan, bad,
                     report);
    });
  }
  auto stale = plan;
  ++stale.regions[kScalar].preferred_entries;
  rejected([&] {
    return Execute(provider, routine, trans, a.view(), b.view(), stale,
                   workspace, report);
  });
  Matrix<T> short_b(2, 2, Layout::kColumnMajor);
  rejected([&] {
    return Execute(provider, routine, trans, a.view(), short_b.view(), plan,
                   workspace, report);
  });
  auto alias = workspace;
  alias.regions[kPacking] = {a.view().data(),
                             a.view().reachable_storage().size(),
                             asc::MemorySpace::kHost};
  rejected([&] {
    return Execute(provider, routine, trans, a.view(), b.view(), plan, alias,
                   report);
  });
}

template <typename T>
void Faults(TestContext& test, const asc::ReferenceLapackProvider& provider,
            Routine routine) {
  Matrix<T> a(3, 2, Layout::kRowMajor);
  Matrix<T> b(3, 2, Layout::kColumnMajor);
  asc::LapackReport report;
  const auto trans = Transpose<T>(false);
  const auto plan =
      Take(Query(provider, routine, trans, a.view(), b.view(), report));
  Scratch<T> scratch(plan, true);
  const auto workspace = scratch.view();
  const auto before_a = a.bytes();
  const auto before_b = b.bytes();
  for (const auto fault :
       {Fault::kNegative, Fault::kMinimumInteger, Fault::kImpossiblePositive,
        Fault::kQueryZero, Fault::kQueryNan}) {
    SetFault(routine, fault);
    const auto result = WithoutAllocation(test, [&] {
      return Query(provider, routine, trans, a.view(), b.view(), report);
    });
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
    a.CheckSame(test, before_a);
    b.CheckSame(test, before_b);
  }
  for (const auto fault :
       {Fault::kNegative, Fault::kMinimumInteger, Fault::kImpossiblePositive}) {
    SetFault(routine, fault);
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, trans, a.view(), b.view(), plan,
                     workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    a.CheckSame(test, before_a);
    b.CheckSame(test, before_b);
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    SetFault(routine, Fault::kQueryImaginary);
    const auto result = WithoutAllocation(test, [&] {
      return Query(provider, routine, trans, a.view(), b.view(), report);
    });
    ASC_DENSE_TEST_CHECK(test, !result.ok());
  }
  SetFault(routine, Fault::kNone);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         std::string_view scalar) {
  constexpr std::array<Problem, 11> kProblems{
      Problem{3, 2, 2, true},      Problem{2, 3, 2, false},
      Problem{1, 1, 1, false},     Problem{4, 4, 3, false},
      Problem{0, 3, 2, false},     Problem{3, 0, 2, false},
      Problem{0, 0, 0, false},     Problem{3, 2, 0, false},
      Problem{131, 129, 2, false}, Problem{3, 2, 7, false},
      Problem{2, 3, 7, false}};
  for (const auto routine :
       {Routine::kGels, Routine::kGelst, Routine::kGetsls}) {
    for (const auto problem : kProblems) {
      for (const bool adjoint : {false, true}) {
        for (const auto a_layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
          for (const auto b_layout :
               {Layout::kColumnMajor, Layout::kRowMajor}) {
            for (const bool preferred : {false, true}) {
              CheckProblem<T>(test, provider, routine, problem, adjoint,
                              a_layout, b_layout, preferred, 1, scalar);
            }
          }
        }
      }
    }
    using Real = asc::DenseBlasRealType<T>;
    for (const long double scale :
         {static_cast<long double>(std::numeric_limits<Real>::min()) / 8,
          static_cast<long double>(std::numeric_limits<Real>::max()) / 32}) {
      for (const bool adjoint : {false, true}) {
        for (const auto problem :
             {Problem{3, 2, 2, true}, Problem{2, 3, 2, false}}) {
          CheckProblem<T>(test, provider, routine, problem, adjoint,
                          Layout::kRowMajor, Layout::kColumnMajor, false, scale,
                          scalar);
        }
      }
    }
    for (const auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
      for (const bool adjoint : {false, true}) {
        NumericalFailure<T>(test, provider, routine, adjoint, layout);
      }
    }
    Failures<T>(test, provider, routine);
    Faults<T>(test, provider, routine);
  }
  for (const auto problem :
       {Problem{17000, 8, 2, false}, Problem{8, 17000, 2, false}}) {
    for (const bool adjoint : {false, true}) {
      for (const bool preferred : {false, true}) {
        CheckProblem<T>(test, provider, Routine::kGetsls, problem, adjoint,
                        Layout::kColumnMajor, Layout::kRowMajor, preferred, 1,
                        scalar);
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider, scalar);
  } else if (scalar == "d") {
    Run<double>(test, provider, scalar);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider, scalar);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, scalar);
  } else {
    return 2;
  }
  return test.Finish();
}
