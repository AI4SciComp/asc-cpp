#include <array>
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
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_scalar_sort.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_sort_test_support.h"
namespace {
namespace sort = asc_scalar_sort_test;
template <typename T>
void Case(sort::TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::extent_t n, asc::LapackSortOrder order, unsigned pattern,
          unsigned seed, bool fidelity, bool audit) {
  sort::Buffer<T> buffer(n, pattern, seed);
  const auto before = buffer.values;
  const bool nan = buffer.HasNan();
  auto expected = before;
  if (fidelity) {
    const char direction =
        order == asc::LapackSortOrder::kIncreasing ? 'I' : 'D';
    const auto native_n = static_cast<lapack_int>(n);
    lapack_int info = std::numeric_limits<lapack_int>::max();
    sort::Native(&direction, &native_n, expected.data() + 1, &info);
    ASC_DENSE_TEST_EQ(test, info, 0);
  }
  const auto view = buffer.View();
  asc::LapackReport report;
  const auto call = [&] { return asc::Lasrt(provider, order, view, report); };
  const auto status = audit ? sort::WithoutAllocation(test, call) : call();
  ASC_DENSE_TEST_EQ(test, status.code(),
                    nan ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n != 0);
  if (report.native_info.has_value()) {
    ASC_DENSE_TEST_EQ(test, *report.native_info, 0);
  }
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    nan ? asc::LapackOutcome::kAccuracyWarning
                        : asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_CHECK(test, sort::Permutation(buffer, before));
  if (fidelity) {
    ASC_DENSE_TEST_CHECK(test, sort::Same(buffer.values, expected));
  } else if (!nan) {
    ASC_DENSE_TEST_CHECK(test, sort::Ordered(buffer, order));
  }
}
template <typename T>
void Run(sort::TestContext& test, std::string_view mode) {
  const auto provider = sort::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "concurrency") {
    std::array<std::thread, 4> threads;
    std::array<int, 4> failures{};
    for (std::size_t worker = 0; worker < threads.size(); ++worker) {
      threads[worker] = std::thread([&, worker] {
        sort::TestContext local;
        for (auto n : sort::kLengths) {
          for (auto order : sort::kOrders) {
            for (unsigned pattern = 0; pattern < 8; ++pattern) {
              Case<T>(local, provider, n, order, pattern,
                      static_cast<unsigned>(worker), false, false);
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
  for (auto n : sort::kLengths) {
    for (auto order : sort::kOrders) {
      for (unsigned pattern = 0; pattern < 8; ++pattern) {
        for (unsigned seed = 0; seed < 16; ++seed) {
          Case<T>(test, provider, n, order, pattern, seed, mode == "fidelity",
                  true);
          ++cases;
        }
      }
    }
  }
  std::printf("scalar sort cases=%zu\n", cases);
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
  sort::TestContext test;
  if (scalar == "s") {
    Run<float>(test, mode);
  } else if (scalar == "d") {
    Run<double>(test, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
