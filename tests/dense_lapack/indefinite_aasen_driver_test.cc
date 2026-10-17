#include <complex>
#include <cstdio>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_driver_numerical_support.h"
#include "indefinite_aasen_solve_fixture.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_solve_test;
using asc_aasen_driver_test::Case;
template <typename T>
int Run(bool he, bool fidelity) {
  base::TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider =
      base::Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto layout : {base::kColumn, base::kRow}) {
      for (auto rhs_layout : {base::kColumn, base::kRow}) {
        for (bool oversized : {false, true}) {
          for (int n : {0, 1, 2, 3, 7, 67}) {
            for (int nrhs : {0, 1, 2, 3}) {
              Case(
                  test, provider,
                  aa::Sample<T>(n, nrhs, he, tri, layout, rhs_layout, 0, false),
                  false, oversized, fidelity);
              ++cases;
            }
          }
          for (int n : {1, 3, 7, 67}) {
            for (int nrhs : {0, 1, 2, 3}) {
              Case(test, provider,
                   aa::Sample<T>(n, nrhs, he, tri, layout, rhs_layout, 0, true),
                   true, oversized, fidelity);
              ++cases;
            }
          }
          constexpr int kScale =
              sizeof(asc::DenseBlasRealType<T>) == 4 ? 50 : 500;
          for (int exponent : {-kScale, kScale}) {
            Case(test, provider,
                 aa::Sample<T>(7, 3, he, tri, layout, rhs_layout, exponent,
                               false),
                 false, oversized, fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf("Aasen checked driver cases=%d failed=%d\n", cases,
              test.Finish());
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
