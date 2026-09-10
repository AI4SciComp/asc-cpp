#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <span>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"
#include "dmd_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_dmd_test;
using support::Take;
using support::TestContext;
using Complex = std::complex<long double>;
using Scaling = asc::LapackDmdScaling;
using Vectors = asc::LapackDmdVectors;
using Extra = asc::LapackDmdExtra;
using Svd = asc::LapackDmdSvd;
template <typename T>
T Operator(asc::extent_t i, asc::extent_t j) {
  if (i >= 2 || j >= 2) {
    return T{};
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    const std::array<T, 4> values{T{1, 1}, T{1, 0}, T{0, 0}, T{3, -1}};
    return values[static_cast<std::size_t>(2 * i + j)];
  } else {
    const std::array<T, 4> values{1, -2, 2, 1};
    return values[static_cast<std::size_t>(2 * i + j)];
  }
}
template <typename T>
void Fixture(support::Problem<T>& p, support::Real<T> scale = 1) {
  for (asc::extent_t j = 0; j < p.x.columns; ++j) {
    for (asc::extent_t i = 0; i < p.x.rows; ++i) {
      p.x.At(i, j) = i < 2 && (j == i || j == 2) ? T{1} : T{};
      p.x.At(i, j) *= scale;
      p.y.At(i, j) = T{};
      for (asc::extent_t k = 0; k < 2; ++k) {
        p.y.At(i, j) +=
            scale * Operator<T>(i, k) * (j == k || j == 2 ? T{1} : T{});
      }
    }
  }
}
template <typename T>
Complex Mode(const support::Problem<T>& p, bool explicit_vectors,
             asc::extent_t i, asc::extent_t j) {
  const auto entry = [&](asc::extent_t column) {
    Complex value{};
    if (explicit_vectors) {
      return support::ToWide(p.z.At(i, column));
    }
    for (asc::extent_t k = 0; k < p.rank; ++k) {
      value +=
          support::ToWide(p.x.At(i, k)) * support::ToWide(p.w.At(k, column));
    }
    return value;
  };
  if constexpr (asc::DenseBlasComplex<T>) {
    return entry(j);
  } else {
    if (p.eigen[static_cast<std::size_t>(j) + 1].imag() > 0) {
      return entry(j) + Complex{0, 1} * entry(j + 1);
    }
    if (p.eigen[static_cast<std::size_t>(j) + 1].imag() < 0) {
      return entry(j - 1) - Complex{0, 1} * entry(j);
    }
    return entry(j);
  }
}
template <typename T>
void CheckModes(TestContext& test, const support::Problem<T>& p,
                asc::LapackDmdOptions options) {
  const long double tolerance =
      256 * std::numeric_limits<support::Real<T>>::epsilon();
  for (asc::extent_t j = 0; j < p.rank; ++j) {
    const auto lambda =
        support::ToWide(p.eigen[static_cast<std::size_t>(j) + 1]);
    const Complex first =
        asc::DenseBlasComplex<T> ? Complex{1, 1} : Complex{1, 2};
    const Complex second =
        asc::DenseBlasComplex<T> ? Complex{3, -1} : Complex{1, -2};
    ASC_DENSE_TEST_CHECK(
        test, std::min(std::abs(lambda - first), std::abs(lambda - second)) <=
                  tolerance);
    if (options.vectors == Vectors::kNone && options.extra != Extra::kExact) {
      continue;
    }
    long double norm = 0;
    long double residual = 0;
    for (asc::extent_t i = 0; i < p.x.rows; ++i) {
      const auto zi = Mode(p, options.vectors == Vectors::kExplicit, i, j);
      Complex applied{};
      for (asc::extent_t k = 0; k < p.x.rows; ++k) {
        applied += support::ToWide(Operator<T>(i, k)) *
                   Mode(p, options.vectors == Vectors::kExplicit, k, j);
      }
      norm += std::norm(zi);
      residual += std::norm(applied - lambda * zi);
    }
    ASC_DENSE_TEST_CHECK(test, std::abs(norm - 1) <= tolerance);
    ASC_DENSE_TEST_CHECK(test, std::sqrt(residual) <= tolerance);
    if (options.residuals) {
      ASC_DENSE_TEST_CHECK(test,
                           p.residual[static_cast<std::size_t>(j) + 1] >= 0);
      ASC_DENSE_TEST_CHECK(
          test, std::abs(p.residual[static_cast<std::size_t>(j) + 1] -
                         std::sqrt(residual)) <= tolerance);
    }
  }
  for (asc::extent_t i = 0; i < p.rank; ++i) {
    for (asc::extent_t j = 0; j < p.rank; ++j) {
      Complex dot{};
      for (asc::extent_t k = 0; k < p.x.rows; ++k) {
        dot += std::conj(support::ToWide(p.x.At(k, i))) *
               support::ToWide(p.x.At(k, j));
      }
      ASC_DENSE_TEST_CHECK(
          test, std::abs(dot - Complex{i == j ? 1.L : 0.L}) <= tolerance);
    }
  }
}
template <typename T>
bool SameBytes(const T& a, const T& b) {
  const auto first = std::as_bytes(std::span(&a, 1));
  const auto second = std::as_bytes(std::span(&b, 1));
  return std::equal(first.begin(), first.end(), second.begin());
}
template <typename T>
void CheckOutputs(TestContext& test, const support::Problem<T>& p,
                  const support::Problem<T>& before,
                  asc::LapackDmdOptions options) {
  const long double tolerance =
      256 * std::numeric_limits<support::Real<T>>::epsilon();
  long double scaled_frobenius = 0;
  for (asc::extent_t j = 0; j < p.x.columns; ++j) {
    long double scale = 1;
    if (options.scaling != Scaling::kNone) {
      scale = 0;
      for (asc::extent_t i = 0; i < p.x.rows; ++i) {
        scale += std::norm(support::ToWide(
            options.scaling == Scaling::kSuccessors ? before.y.At(i, j)
                                                    : before.x.At(i, j)));
      }
      scale = std::sqrt(scale);
    }
    for (asc::extent_t i = 0; i < p.x.rows; ++i) {
      scaled_frobenius += std::norm(support::ToWide(before.x.At(i, j)) / scale);
      if (!options.residuals) {
        ASC_DENSE_TEST_CHECK(test, std::abs(support::ToWide(p.y.At(i, j)) -
                                            support::ToWide(before.y.At(i, j)) /
                                                scale) <= tolerance);
      }
    }
  }
  long double singular_frobenius = 0;
  for (asc::extent_t j = 0; j < p.x.columns; ++j) {
    const auto value = p.singular[static_cast<std::size_t>(j) + 1];
    ASC_DENSE_TEST_CHECK(test, value >= 0);
    singular_frobenius += static_cast<long double>(value) * value;
    if (j > 0) {
      ASC_DENSE_TEST_CHECK(
          test, value <= p.singular[static_cast<std::size_t>(j)] + tolerance);
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, std::abs(singular_frobenius - scaled_frobenius) <= tolerance);
  if (options.extra != Extra::kNone) {
    for (asc::extent_t j = 0; j < p.rank; ++j) {
      for (asc::extent_t i = 0; i < p.x.rows; ++i) {
        Complex expected{};
        for (asc::extent_t k = 0; k < p.x.rows; ++k) {
          Complex u = support::ToWide(p.x.At(k, j));
          if (options.extra == Extra::kExact) {
            u = Complex{};
            for (asc::extent_t h = 0; h < p.rank; ++h) {
              u +=
                  support::ToWide(p.x.At(k, h)) * support::ToWide(p.w.At(h, j));
            }
          }
          expected += support::ToWide(Operator<T>(i, k)) * u;
        }
        ASC_DENSE_TEST_CHECK(test, std::abs(support::ToWide(p.b.At(i, j)) -
                                            expected) <= tolerance);
      }
    }
  } else {
    ASC_DENSE_TEST_CHECK(test, SameBytes(p.b.data, before.b.data));
  }
  if (options.vectors != Vectors::kExplicit) {
    ASC_DENSE_TEST_CHECK(test, SameBytes(p.z.data, before.z.data));
  }
  if (!options.residuals) {
    ASC_DENSE_TEST_CHECK(test, SameBytes(p.residual, before.residual));
  }
  if (options.vectors == Vectors::kNone && options.extra != Extra::kExact) {
    ASC_DENSE_TEST_CHECK(test, SameBytes(p.w.data, before.w.data));
  }
  for (std::size_t i = 0; i < p.eigen.size(); ++i) {
    if (i == 0 || i > static_cast<std::size_t>(p.rank)) {
      ASC_DENSE_TEST_EQ(test, p.eigen[i], before.eigen[i]);
      ASC_DENSE_TEST_EQ(test, p.residual[i], before.residual[i]);
    }
    if (i == 0 || i > static_cast<std::size_t>(p.x.columns)) {
      ASC_DENSE_TEST_EQ(test, p.singular[i], before.singular[i]);
    }
  }
}
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::LapackDmdOptions options,
          const std::array<asc::DenseBlasLayout, 6>& layouts, bool preferred,
          bool audit = true,
          const asc::LapackWorkspacePlan* supplied_plan = nullptr,
          support::Real<T> input_scale = 1) {
  support::Problem<T> p(4, 3, layouts);
  Fixture(p, input_scale);
  const auto before = p;
  const auto tolerance =
      support::Real<T>{32} * std::numeric_limits<support::Real<T>>::epsilon();
  const auto queried =
      Take(asc::QueryGedmdWorkspace(provider, options, p.Buffers(), tolerance));
  const auto& plan = supplied_plan == nullptr ? queried : *supplied_plan;
  support::Scratch<T> scratch(plan, preferred);
  asc::LapackReport report;
  const auto call = [&] {
    return asc::Gedmd(provider, options, p.Buffers(), tolerance, p.rank, plan,
                      scratch.workspace, report);
  };
  const auto status = audit ? support::WithoutAllocation(test, call) : call();
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_EQ(test, p.rank, 2);
  if (!status.ok() || p.rank != 2) {
    return;
  }
  CheckModes(test, p, options);
  CheckOutputs(test, p, before, options);
  for (asc::extent_t i = 0; i < p.x.rows; ++i) {
    ASC_DENSE_TEST_EQ(test, p.x.At(i, 2), before.x.At(i, 2));
    ASC_DENSE_TEST_EQ(test, p.z.At(i, 2), before.z.At(i, 2));
    ASC_DENSE_TEST_EQ(test, p.b.At(i, 2), before.b.At(i, 2));
  }
  for (const auto* matrix : {&p.x, &p.y, &p.z, &p.b, &p.w, &p.s}) {
    matrix->Guards(test);
  }
  scratch.Guards(test);
}
template <typename T>
void SerialMatrix(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  // Finite matrix:4 SVDs,4 scalings,3 vector modes,3 extras, all legal
  // residual choices,3 rank selectors and minimum/preferred workspace.
  // Complementary layouts exercise each independent matrix packing path.
  for (const auto svd : {Svd::kBidiagonalQr, Svd::kDivideAndConquer,
                         Svd::kQrPreconditioned, Svd::kJacobi}) {
    for (const auto scaling :
         {Scaling::kNone, Scaling::kSnapshots, Scaling::kConsistentSnapshots,
          Scaling::kSuccessors}) {
      for (const auto vectors :
           {Vectors::kNone, Vectors::kExplicit, Vectors::kFactored}) {
        for (const auto extra :
             {Extra::kNone, Extra::kRefinement, Extra::kExact}) {
          for (const bool residuals : {false, true}) {
            if (residuals && vectors != Vectors::kExplicit) {
              continue;
            }
            for (const asc::index_t selector : {-1, -2, 2}) {
              const asc::LapackDmdOptions options{scaling, vectors,   extra,
                                                  svd,     residuals, selector};
              for (const bool preferred : {false, true}) {
                for (const bool reverse : {false, true}) {
                  const auto a = reverse ? support::kRow : support::kColumn;
                  const auto b = reverse ? support::kColumn : support::kRow;
                  const std::array layouts{a, b, a, b, a, b};
                  Case<T>(test, provider, options, layouts, preferred);
                }
              }
            }
          }
        }
      }
    }
  }
}
template <typename T>
void Concurrency(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  // The real provider executes in every worker. Allocation observation and
  // fault injection are excluded from the concurrent section.
  const std::array parallel_layouts{support::kColumn, support::kRow,
                                    support::kColumn, support::kRow,
                                    support::kColumn, support::kRow};
  const auto plan_for = [&](Svd svd) {
    support::Problem<T> p(4, 3, parallel_layouts);
    const asc::LapackDmdOptions options{
        Scaling::kNone, Vectors::kExplicit, Extra::kRefinement, svd, true, -1};
    const auto tolerance =
        support::Real<T>{32} * std::numeric_limits<support::Real<T>>::epsilon();
    return Take(
        asc::QueryGedmdWorkspace(provider, options, p.Buffers(), tolerance));
  };
  const std::array shared_plans{
      plan_for(Svd::kBidiagonalQr), plan_for(Svd::kDivideAndConquer),
      plan_for(Svd::kQrPreconditioned), plan_for(Svd::kJacobi)};
  std::array<int, 4> failures{};
  std::array<std::thread, 4> workers;
  for (std::size_t i = 0; i < workers.size(); ++i) {
    workers[i] = std::thread([&, i] {
      TestContext local;
      const auto independent = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      const asc::LapackDmdOptions options{Scaling::kNone,
                                          Vectors::kExplicit,
                                          Extra::kRefinement,
                                          static_cast<Svd>(i + 1),
                                          true,
                                          -1};
      const std::array layouts{support::kColumn, support::kRow,
                               support::kColumn, support::kRow,
                               support::kColumn, support::kRow};
      for (int repetition = 0; repetition < 4; ++repetition) {
        Case<T>(local, independent, options, layouts, true, false,
                &shared_plans[i], static_cast<support::Real<T>>(i + 1));
      }
      failures[i] = local.Finish();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (const int failure : failures) {
    ASC_DENSE_TEST_EQ(test, failure, 0);
  }
}
template <typename T>
int Run() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  SerialMatrix<T>(test, provider);
  Concurrency<T>(test, provider);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>();
  }
  if (scalar == "d") {
    return Run<double>();
  }
  if (scalar == "c") {
    return Run<std::complex<float>>();
  }
  if (scalar == "z") {
    return Run<std::complex<double>>();
  }
  return 2;
}
