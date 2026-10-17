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
#include "asc/dense/providers/lapack_rotation.h"
#include "installed_lu/normal_return_guard.h"
#include "positive_rotation_test_support.h"

namespace {
namespace positive_rotation = asc_positive_rotation_test;
namespace numerical = asc_positive_rotation_numerical;
using positive_rotation::TestContext;

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider, T f,
          T g, bool fidelity, bool audit) {
  positive_rotation::Output<T> c;
  positive_rotation::Output<T> s;
  positive_rotation::Output<T> r;
  const auto cv = c.View();
  const auto sv = s.View();
  const auto rv = r.View();
  asc::LapackReport report;
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto call = [&] {
      return asc::Lartgp(provider, f, g, cv, sv, rv, report);
    };
    const auto status =
        audit ? positive_rotation::WithoutAllocation(test, call) : call();
    ASC_DENSE_TEST_EQ(test, status.code(),
                      std::isfinite(r.values[1]) ? asc::ErrorCode::kOk
                                                 : asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
    ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()),
                      (std::is_same_v<T, float> ? "slartgp" : "dlartgp"));
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      std::isfinite(r.values[1])
                          ? asc::LapackOutcome::kSuccess
                          : asc::LapackOutcome::kAccuracyWarning);
    if (fidelity) {
      T nf = f;
      T ng = g;
      T nc{};
      T ns{};
      T nr{};
      positive_rotation::Native(nf, ng, nc, ns, nr);
      ASC_DENSE_TEST_EQ(test, numerical::Bits(nf), numerical::Bits(f));
      ASC_DENSE_TEST_EQ(test, numerical::Bits(ng), numerical::Bits(g));
      ASC_DENSE_TEST_EQ(test, numerical::Bits(c.values[1]),
                        numerical::Bits(nc));
      ASC_DENSE_TEST_EQ(test, numerical::Bits(s.values[1]),
                        numerical::Bits(ns));
      ASC_DENSE_TEST_EQ(test, numerical::Bits(r.values[1]),
                        numerical::Bits(nr));
    } else {
      ASC_DENSE_TEST_CHECK(test, numerical::Accurate(f, g, c.values[1],
                                                     s.values[1], r.values[1]));
    }
    for (const auto* output : {&c, &s, &r}) {
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
      for (int exponent = -20; exponent <= 20; ++exponent) {
        Case(local, provider, std::ldexp(T{3}, exponent),
             std::ldexp(T{-4}, exponent), false, false);
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
  const auto provider = positive_rotation::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "concurrency") {
    Concurrent<T>(test, provider);
    return;
  }
  for (T f : numerical::Values<T>()) {
    for (T g : numerical::Values<T>()) {
      Case(test, provider, f, g, mode == "fidelity", true);
    }
  }
  Case(test, provider, T{3}, T{4}, mode == "fidelity", true);
  std::printf(
      "1937 real positive_rotation fixtures, first and reused output calls\n");
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
