#include <array>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_machine.h"
#include "installed_lu/normal_return_guard.h"
#include "machine_constants_test_support.h"

namespace {
namespace machine_constants = asc_machine_constants_test;
namespace numerical = asc_machine_constants_numerical;
using machine_constants::TestContext;

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          numerical::Case<T> fixture, T old, bool fidelity, bool audit) {
  machine_constants::Output<T> c;
  c.values[1] = old;
  const auto cv = c.View();
  asc::LapackReport report;
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto call = [&] {
      return asc::Lamch(provider, fixture.parameter, cv, report);
    };
    const auto status =
        audit ? machine_constants::WithoutAllocation(test, call) : call();
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kOk);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
    ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()),
                      (std::is_same_v<T, float> ? "slamch" : "dlamch"));
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    if (fidelity) {
      char selector = fixture.selector;
      const T expected = machine_constants::Native<T>(selector);
      ASC_DENSE_TEST_EQ(test, selector, fixture.selector);
      ASC_DENSE_TEST_EQ(test, numerical::Bits(c.values[1]),
                        numerical::Bits(expected));
    } else {
      ASC_DENSE_TEST_EQ(test, numerical::Bits(c.values[1]),
                        numerical::Bits(fixture.expected));
    }
    for (const auto* output : {&c}) {
      ASC_DENSE_TEST_EQ(test, output->values[0], T{71});
      ASC_DENSE_TEST_EQ(test, output->values[2], T{73});
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
      for (int repeat = 0; repeat < 16; ++repeat) {
        for (const auto fixture : numerical::Cases<T>()) {
          Case(local, provider, fixture, T{-19}, false, false);
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
}

template <typename T>
void Run(TestContext& test, std::string_view mode) {
  const auto provider = machine_constants::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "concurrency") {
    Concurrent<T>(test, provider);
    return;
  }
  for (const auto fixture : numerical::Cases<T>()) {
    for (const T old : {T{-19}, -T{0}, std::numeric_limits<T>::quiet_NaN()}) {
      Case(test, provider, fixture, old, mode == "fidelity", true);
    }
  }
  std::printf("30 LAMCH selector/output fixtures, first and reused calls\n");
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
