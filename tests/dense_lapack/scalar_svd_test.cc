#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <thread>
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_svd_test_support.h"

namespace {
namespace scalar_svd = asc_scalar_svd_test;
namespace numerical = asc_scalar_svd_numerical;
using scalar_svd::TestContext;

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider, T f,
          T g, T h, bool fidelity, bool audit) {
  scalar_svd::Output<T> output;
  const auto views = output.Views();
  asc::LapackReport report;
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto call = [&] {
      return scalar_svd::Execute(provider, f, g, h, views, report);
    };
    const auto status =
        audit ? scalar_svd::WithoutAllocation(test, call) : call();
    ASC_DENSE_TEST_EQ(test, status.code(),
                      std::isfinite(output.values[1][1])
                          ? asc::ErrorCode::kOk
                          : asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
    ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()),
                      (std::is_same_v<T, float> ? "slasv2" : "dlasv2"));
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      std::isfinite(output.values[1][1])
                          ? asc::LapackOutcome::kSuccess
                          : asc::LapackOutcome::kAccuracyWarning);
    if (fidelity) {
      T nf = f;
      T ng = g;
      T nh = h;
      std::array<T, 6> native{};
      scalar_svd::Native(nf, ng, nh, native[0], native[1], native[2], native[3],
                         native[4], native[5]);
      ASC_DENSE_TEST_EQ(test, numerical::Bits(nf), numerical::Bits(f));
      ASC_DENSE_TEST_EQ(test, numerical::Bits(ng), numerical::Bits(g));
      ASC_DENSE_TEST_EQ(test, numerical::Bits(nh), numerical::Bits(h));
      for (std::size_t i = 0; i < native.size(); ++i) {
        ASC_DENSE_TEST_EQ(test, numerical::Bits(output.values[i][1]),
                          numerical::Bits(native[i]));
      }
    } else {
      ASC_DENSE_TEST_CHECK(
          test,
          numerical::Accurate(f, g, h, output.values[0][1], output.values[1][1],
                              output.values[2][1], output.values[3][1],
                              output.values[4][1], output.values[5][1]));
    }
    for (const auto& values : output.values) {
      ASC_DENSE_TEST_EQ(test, values[0], T{71});
      ASC_DENSE_TEST_EQ(test, values[2], T{73});
    }
  }
}

template <typename T>
void Concurrent(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  std::array<std::thread, 4> threads;
  std::array<int, 4> failures{};
  for (std::size_t worker = 0; worker < threads.size(); ++worker) {
    threads[worker] = std::thread([&, worker] {
      TestContext local;
      for (int exponent = -20; exponent <= 20; ++exponent) {
        Case(local, provider, std::ldexp(T{3}, exponent),
             std::ldexp(T{-4}, exponent), std::ldexp(T{5}, exponent), false,
             false);
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
void Run(TestContext& test, std::string_view mode) {
  const auto provider = scalar_svd::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "concurrency") {
    Concurrent<T>(test, provider);
    return;
  }
  numerical::ForEach<T>([&](T f, T g, T h) {
    Case(test, provider, f, g, h, mode == "fidelity", true);
  });
  std::printf(
      "38912 LASV2 finite/single-infinity fixtures, first and reused "
      "outputs\n");
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
  if (scalar == "s") {
    Run<float>(test, mode);
  } else if (scalar == "d") {
    Run<double>(test, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
