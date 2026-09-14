#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "triangular_conversion_test_support.h"
namespace {
namespace tc = asc_triangular_conversion_test;
using tc::TestContext;
template <typename Input>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::extent_t n, asc::DenseBlasTriangle triangle,
          asc::DenseBlasLayout il, asc::DenseBlasLayout ol, std::size_t shift,
          bool fidelity, bool audit) {
  tc::Problem<Input> p(n, triangle, il, ol);
  tc::Initialize(p, shift);
  const auto before = p;
  const auto plan = tc::Take(p.Query(provider));
  tc::Scratch<Input> scratch;
  const auto workspace = scratch.Workspace(plan);
  ASC_DENSE_TEST_EQ(test, plan.regions[tc::kScratch].minimum_entries, n * n);
  ASC_DENSE_TEST_EQ(test, plan.regions[tc::kLayout].minimum_entries,
                    il == tc::kRow ? n * n : 0);
  asc::LapackReport report;
  const auto call = [&] { return p.Run(provider, plan, workspace, report); };
  const auto status = audit ? tc::WithoutAllocation(test, call) : call();
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, n > 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n > 0);
  if (n > 0) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  }
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  tc::Rhs<Input> original(n, n, tc::kColumn);
  tc::Rhs<tc::Output<Input>> raw(n, n, tc::kColumn);
  for (asc::extent_t j = 0; j < n; ++j) {
    for (asc::extent_t i = 0; i < n; ++i) {
      original.At(i, j) = before.input.At(i, j);
    }
  }
  if (fidelity && n > 0) {
    lapack_int info = 0;
    tc::Native(triangle == tc::kUpper ? 'U' : 'L', static_cast<lapack_int>(n),
               original.View().data(), static_cast<lapack_int>(original.ld),
               raw.View().data(), static_cast<lapack_int>(raw.ld), info);
    ASC_DENSE_TEST_EQ(test, info, 0);
  }
  for (asc::extent_t j = 0; j < n; ++j) {
    for (asc::extent_t i = 0; i < n; ++i) {
      const auto wide = tc::ToWide(before.input.At(i, j));
      auto expected = before.output.At(i, j);
      if (tc::Selected(triangle, i, j)) {
        expected = fidelity
                       ? raw.At(i, j)
                       : tc::Value<tc::Output<Input>>(wide.real(), wide.imag());
      }
      ASC_DENSE_TEST_CHECK(test, tc::Same(p.output.At(i, j), expected));
    }
  }
  ASC_DENSE_TEST_EQ(test, p.input.data, before.input.data);
  p.output.Guards(test);
  raw.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename Input>
void Exceptional(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, asc::DenseBlasLayout il,
                 asc::DenseBlasLayout ol) {
  const std::array<double, 5> values{std::numeric_limits<double>::max(),
                                     -std::numeric_limits<double>::max(),
                                     std::numeric_limits<double>::infinity(),
                                     -std::numeric_limits<double>::infinity(),
                                     std::numeric_limits<double>::quiet_NaN()};
  for (double special : values) {
    for (int component = 0; component < (asc::DenseBlasComplex<Input> ? 2 : 1);
         ++component) {
      tc::Problem<Input> p(5, triangle, il, ol);
      tc::Initialize(p);
      p.input.At(4, 4) = component == 0 ? tc::Value<Input>(special, 0)
                                        : tc::Value<Input>(0, special);
      const auto before = p;
      const auto plan = tc::Take(p.Query(provider));
      tc::Scratch<Input> scratch;
      const auto workspace = scratch.Workspace(plan);
      asc::LapackReport report;
      const auto status = tc::WithoutAllocation(
          test, [&] { return p.Run(provider, plan, workspace, report); });
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        asc::LapackOutcome::kAccuracyWarning);
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1),
                        std::isnan(special) ? 0 : 1);
      if (std::isnan(special)) {
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kComplete);
        ASC_DENSE_TEST_CHECK(test, tc::Same(p.output.At(4, 4),
                                            tc::Value<tc::Output<Input>>(
                                                component == 0 ? special : 0,
                                                component == 1 ? special : 0)));
      } else {
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnchanged);
        ASC_DENSE_TEST_EQ(test, p.output.data, before.output.data);
      }
      for (std::size_t i = 0; i < p.input.data.size(); ++i) {
        ASC_DENSE_TEST_CHECK(test,
                             tc::Same(p.input.data[i], before.input.data[i]));
      }
      p.output.Guards(test);
      scratch.Guards(test, workspace);
    }
  }
}
template <typename Input>
void Run(TestContext& test, std::string_view mode) {
  const auto provider = tc::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "concurrency") {
    std::array<std::thread, 4> threads;
    std::array<int, 4> failures{};
    for (std::size_t worker = 0; worker < threads.size(); ++worker) {
      threads[worker] = std::thread([&, worker] {
        TestContext local;
        for (int repeat = 0; repeat < 4; ++repeat) {
          for (auto triangle : {tc::kUpper, tc::kLower}) {
            for (auto il : {tc::kColumn, tc::kRow}) {
              for (auto ol : {tc::kColumn, tc::kRow}) {
                Case<Input>(local, provider, 5, triangle, il, ol, worker, false,
                            false);
              }
            }
          }
        }
        failures[worker] = local.Finish();
      });
    }
    for (auto& thread : threads) {
      thread.join();
    }
    for (int failure : failures) {
      ASC_DENSE_TEST_EQ(test, failure, 0);
    }
    return;
  }
  std::size_t cases = 0;
  for (auto triangle : {tc::kUpper, tc::kLower}) {
    for (auto il : {tc::kColumn, tc::kRow}) {
      for (auto ol : {tc::kColumn, tc::kRow}) {
        for (asc::extent_t n : {0, 1, 2, 5, 13}) {
          for (std::size_t shift = 0; shift < 16; ++shift) {
            Case<Input>(test, provider, n, triangle, il, ol, shift,
                        mode == "fidelity", true);
            ++cases;
          }
        }
        if (mode == "mathematical") {
          Exceptional<Input>(test, provider, triangle, il, ol);
        }
      }
    }
  }
  std::printf("triangular conversion %zu cases\n", cases);
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity" && mode != "concurrency") {
    return 2;
  }
  TestContext test;
  if (scalar == "d") {
    Run<double>(test, mode);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
