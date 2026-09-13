#include <complex>
#include <cstdio>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_fixture.h"
#include "indefinite_aasen_numerical_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_test;
using aa::Case;
using base::TestContext;
template <typename T>
int Run(bool he, bool fidelity) {
  TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider =
      base::Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  for (const auto tri : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (int work : {0, 1, 2}) {
        for (int n : {0, 1, 2, 3, 7, 67}) {
          Case(test, provider, aa::Sample<T>(n, he, tri, layout, 0, false),
               work, fidelity);
          ++cases;
        }
        for (int n : {1, 7, 67}) {
          Case(test, provider, aa::Sample<T>(n, he, tri, layout, 0, true), work,
               fidelity);
          ++cases;
        }
        const int exponent = sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
        for (int scale : {-exponent, exponent}) {
          Case(test, provider, aa::Sample<T>(7, he, tri, layout, scale, false),
               work, fidelity);
          ++cases;
        }
      }
    }
  }
  std::printf("Aasen checked cases=%d mode=%s\n", cases,
              fidelity ? "fidelity" : "mathematical");
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
