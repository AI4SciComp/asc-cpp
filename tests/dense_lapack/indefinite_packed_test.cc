#include <complex>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>

#include "../../src/dense/lapack/internal_indefinite_packed_counts.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_numerical_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
namespace {
namespace packed = asc_packed_indefinite_test;
void Counts(packed::TestContext& test) {
  namespace counts = asc::internal_indefinite_packed_counts;
  for (const std::int64_t limit :
       {std::int64_t{INT32_MAX}, std::int64_t{INT64_MAX}}) {
    for (bool upper : {false, true}) {
      for (int n : {0, 1, 2, 3, 4, 5}) {
        ASC_DENSE_TEST_CHECK(test, counts::Factor(n, upper, limit).ok());
      }
      const std::int64_t last =
          (limit == INT32_MAX ? 46340 : 3037000499LL) + static_cast<int>(upper);
      ASC_DENSE_TEST_CHECK(test, counts::Factor(last, upper, limit).ok());
      ASC_DENSE_TEST_EQ(test, counts::Factor(last + 1, upper, limit).code(),
                        asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_EQ(test, counts::Factor(limit, upper, limit).code(),
                        asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_EQ(test, counts::Factor(-1, upper, limit).code(),
                        asc::ErrorCode::kInvalidArgument);
    }
  }
  ASC_DENSE_TEST_EQ(test, counts::Factor(3, true, 42).code(),
                    asc::ErrorCode::kInvalidArgument);
}
template <typename T>
int Run(bool he, bool fidelity) {
  packed::TestContext test;
  Counts(test);
  const auto context = asc::ExecutionContext::Serial();
  const auto provider =
      packed::Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  for (auto tri : {packed::kUpper, packed::kLower}) {
    for (auto layout : {packed::kColumn, packed::kRow}) {
      for (int n : {0, 1, 2, 3, 4, 5, 8, 17, 65}) {
        for (int exponent : {-20, 0, 20}) {
          for (int kind : {0, 1, 2, 3}) {
            packed::Sample<T> sample(n, he, tri, layout, exponent);
            sample.Reset(kind);
            packed::Case(test, provider, std::move(sample), fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf("Packed checked cases=%d\n", cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
  if (scalar == "s") {
    return Run<float>(false, fidelity);
  }
  if (scalar == "d") {
    return Run<double>(false, fidelity);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false, fidelity);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false, fidelity);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true, fidelity);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true, fidelity);
  }
  return 2;
}
