#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_norm_test_support.h"
namespace {
namespace norm = asc_scalar_norm_test;
using norm::TestContext;
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider, T x,
          T y, T z, bool three, bool fidelity, bool audit) {
  norm::Output<T> output;
  const auto view = output.View();
  asc::LapackReport report;
  const auto call = [&] {
    return norm::Execute(provider, x, y, z, three, view, report);
  };
  const auto status = audit ? norm::WithoutAllocation(test, call) : call();
  const T expected = norm::Expected(x, y, z, three);
  ASC_DENSE_TEST_EQ(test, status.code(),
                    std::isfinite(expected) ? asc::ErrorCode::kOk
                                            : asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    std::isfinite(expected)
                        ? asc::LapackOutcome::kSuccess
                        : asc::LapackOutcome::kAccuracyWarning);
  if (fidelity) {
    const T raw = norm::Native(x, y, z, three);
    ASC_DENSE_TEST_EQ(test, norm::Bits(output.values[1]), norm::Bits(raw));
  } else {
    ASC_DENSE_TEST_CHECK(test, norm::Accurate(output.values[1], expected));
  }
  ASC_DENSE_TEST_EQ(test, output.values[0], T{71});
  ASC_DENSE_TEST_EQ(test, output.values[2], T{73});
}
template <typename T>
void Concurrent(TestContext& test, const asc::ReferenceLapackProvider& provider,
                bool three) {
  std::array<std::thread, 4> threads;
  std::array<int, 4> failures{};
  for (std::size_t worker = 0; worker < threads.size(); ++worker) {
    threads[worker] = std::thread([&, worker] {
      TestContext local;
      for (int exponent = -20; exponent <= 20; ++exponent) {
        Case(local, provider, std::ldexp(T{3}, exponent),
             std::ldexp(T{4}, exponent), std::ldexp(T{12}, exponent), three,
             false, false);
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
}
template <typename T>
void Run(TestContext& test, bool three, std::string_view mode) {
  const auto provider = norm::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "concurrency") {
    Concurrent<T>(test, provider, three);
    return;
  }
  const auto values = norm::Values<T>();
  std::size_t count = 0;
  for (T x : values) {
    for (T y : values) {
      if (three) {
        for (T z : values) {
          Case(test, provider, x, y, z, true, mode == "fidelity", true);
          ++count;
        }
      } else {
        Case(test, provider, x, y, T{}, false, mode == "fidelity", true);
        ++count;
      }
    }
  }
  // Pythagorean triples add an independent exactly known mathematical result.
  norm::Output<T> output;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, norm::Execute(provider, T{3}, T{4}, T{12}, three,
                                           output.View(), report)
                                 .ok());
  ASC_DENSE_TEST_CHECK(test,
                       norm::Accurate(output.values[1], three ? T{13} : T{5}));
  std::printf("scalar norm cases=%zu\n", count);
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
  if (scalar == "s2" || scalar == "s3") {
    Run<float>(test, scalar == "s3", mode);
  } else if (scalar == "d2" || scalar == "d3") {
    Run<double>(test, scalar == "d3", mode);
  } else {
    return 2;
  }
  return test.Finish();
}
