#include <complex>
#include <cstdio>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_fixture.h"
#include "indefinite_rk_numerical_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_rk_test::Case;
using asc_rk_test::kColumn;
using asc_rk_test::kLower;
using asc_rk_test::kRow;
using asc_rk_test::kUpper;
using asc_rk_test::Sample;
using asc_rk_test::Take;
using asc_rk_test::TestContext;

template <typename T>
int Run(bool hermitian, bool fidelity) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int n : {0, 1, 2, 3, 7, 67}) {
    for (const auto triangle : {kUpper, kLower}) {
      for (const auto layout : {kColumn, kRow}) {
        for (bool singular : {false, true}) {
          for (bool blocked : {false, true}) {
            for (int mode = 0; mode < (blocked ? 4 : 1); ++mode) {
              Case(test, provider,
                   Sample<T>(n, hermitian, triangle, layout, 0, singular),
                   singular, blocked, mode, fidelity);
              ++cases;
            }
          }
        }
      }
    }
  }
  constexpr int kExponent = sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
  for (int scale : {-kExponent, kExponent}) {
    for (const auto triangle : {kUpper, kLower}) {
      for (const auto layout : {kColumn, kRow}) {
        for (bool blocked : {false, true}) {
          for (int mode = 0; mode < (blocked ? 4 : 1); ++mode) {
            Case(test, provider,
                 Sample<T>(7, hermitian, triangle, layout, scale, false), false,
                 blocked, mode, fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf("RK factor cases=%d mode=%s\n", cases,
              fidelity ? "fidelity" : "mathematical");
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
  const std::string_view scalar(argv[1]);
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
