#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <thread>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_vector_conjugation.h"
#include "installed_lu/normal_return_guard.h"
#include "vector_conjugation_test_support.h"
namespace {
namespace conj = asc_vector_conjugation_test;
template <typename T>
void Case(conj::TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::extent_t n, asc::stride_t increment, std::complex<T> value,
          bool fidelity, bool audit) {
  conj::Buffer<T> buffer(n, increment, value);
  const auto before = buffer.values;
  auto expected = buffer.Expected(n, increment);
  if (fidelity) {
    expected = before;
    const auto raw_n = static_cast<lapack_int>(n);
    const auto raw_increment = static_cast<lapack_int>(increment);
    conj::Native(&raw_n, expected.data() + 1, &raw_increment);
  }
  const auto view = buffer.View();
  asc::LapackReport report;
  const auto call = [&] {
    return asc::Lacgv(provider, n, increment, view, report);
  };
  const auto status = audit ? conj::WithoutAllocation(test, call) : call();
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ASC_DENSE_TEST_CHECK(test, conj::Same(buffer.values[i], expected[i]));
  }
  // Conjugation is an involution even for signed zero, infinities and NaNs.
  const auto repeated = audit ? conj::WithoutAllocation(test, call) : call();
  ASC_DENSE_TEST_CHECK(test, repeated.ok());
  for (std::size_t i = 0; i < before.size(); ++i) {
    ASC_DENSE_TEST_CHECK(test, conj::Same(buffer.values[i], before[i]));
  }
}
template <typename T>
void Run(conj::TestContext& test, std::string_view mode) {
  const auto provider = conj::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "concurrency") {
    std::array<std::thread, 4> threads;
    std::array<int, 4> failures{};
    for (std::size_t worker = 0; worker < threads.size(); ++worker) {
      threads[worker] = std::thread([&, worker] {
        conj::TestContext local;
        for (asc::extent_t n : {0, 1, 2, 5, 13}) {
          for (asc::stride_t increment : {-5, -2, -1, 0, 1, 2, 5}) {
            Case(local, provider, n, increment,
                 std::complex<T>{static_cast<T>(worker), -T{3}}, false, false);
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
  for (T real : conj::Values<T>()) {
    for (T imaginary : conj::Values<T>()) {
      for (asc::extent_t n : {0, 1, 2, 5, 13}) {
        for (asc::stride_t increment : {-5, -2, -1, 0, 1, 2, 5}) {
          Case(test, provider, n, increment, std::complex<T>{real, imaginary},
               mode == "fidelity", true);
          ++cases;
        }
      }
    }
  }
  std::printf("vector conjugation cases=%zu, each repeated twice\n", cases);
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
  conj::TestContext test;
  if (scalar == "c") {
    Run<float>(test, mode);
  } else if (scalar == "z") {
    Run<double>(test, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
