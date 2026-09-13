#include <complex>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>

#include "../../src/dense/lapack/internal_indefinite_aasen_two_stage_solve_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_solve_fixture.h"
#include "indefinite_aasen_two_stage_solve_numerical_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_solve_test;
using aa::Case;
void CountRegression(base::TestContext& test) {
  namespace counts = asc::internal_indefinite_aasen_two_stage_solve_counts;
  for (const std::int64_t limit :
       {std::int64_t{INT32_MAX}, std::int64_t{INT64_MAX}}) {
    ASC_DENSE_TEST_CHECK(test, counts::Solve(2, 0, 2, 8, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(0, 3, 1, 0, 1, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Solve(2, 0, 2, 7, 1, limit).code(),
                      asc::ErrorCode::kShape);
    ASC_DENSE_TEST_EQ(test, counts::Solve(2, 1, 2, 8, 1, limit).code(),
                      asc::ErrorCode::kShape);
    ASC_DENSE_TEST_CHECK(test, counts::Solve(2, 1, 2, 8, 2, limit).ok());
    ASC_DENSE_TEST_EQ(
        test, counts::Solve(limit / 4 + 1, 0, 1, limit, 1, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Solve(2, limit, 2, 8, 2, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test, counts::Solve(7, 33, 7, 28, (limit - 1) / 33 + 1, limit).code(),
        asc::ErrorCode::kOverflow);
  }
}
template <typename T>
int Run(bool he, bool fidelity) {
  base::TestContext test;
  CountRegression(test);
  const auto context = asc::ExecutionContext::Serial();
  const auto provider =
      base::Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto layout : {base::kColumn, base::kRow}) {
      for (auto rhs_layout : {base::kColumn, base::kRow}) {
        for (int capacity : {0, 2}) {
          for (int n : {0, 1, 2, 3, 7, 67}) {
            for (int nrhs : {0, 1, 2, 3}) {
              Case(test, provider,
                   aa::Sample<T>(n, nrhs, he, tri == base::kUpper,
                                 layout == base::kRow, rhs_layout == base::kRow,
                                 capacity, capacity, false),
                   fidelity);
              ++cases;
            }
          }
          for (int n : {1, 3, 7, 67}) {
            for (int nrhs : {1, 2, 3}) {
              Case(test, provider,
                   aa::Sample<T>(n, nrhs, he, tri == base::kUpper,
                                 layout == base::kRow, rhs_layout == base::kRow,
                                 capacity, capacity, true),
                   fidelity);
              ++cases;
            }
          }
          constexpr int kScale =
              sizeof(asc::DenseBlasRealType<T>) == 4 ? 50 : 500;
          for (int exponent : {-kScale, kScale}) {
            aa::Sample<T> sample(7, 3, he, tri == base::kUpper,
                                 layout == base::kRow, rhs_layout == base::kRow,
                                 capacity, capacity);
            sample.Scale(exponent);
            Case(test, provider, std::move(sample), fidelity);
            ++cases;
          }
        }
        for (int band : {0, 1, 2}) {
          for (int work : {0, 1, 2}) {
            Case(test, provider,
                 aa::Sample<T>(193, 3, he, tri == base::kUpper,
                               layout == base::kRow, rhs_layout == base::kRow,
                               band, work),
                 fidelity);
            ++cases;
          }
        }
        for (int capacity : {0, 2}) {
          Case(test, provider,
               aa::Sample<T>(7, 33, he, tri == base::kUpper,
                             layout == base::kRow, rhs_layout == base::kRow,
                             capacity, capacity),
               fidelity);
          ++cases;
        }
      }
    }
  }
  std::printf("Two-stage Aasen checked solve cases=%d\n", cases);
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
