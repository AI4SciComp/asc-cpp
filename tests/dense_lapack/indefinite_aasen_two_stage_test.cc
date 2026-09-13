#include <complex>
#include <cstdio>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_fixture.h"
#include "indefinite_aasen_two_stage_numerical_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace aa = asc_aasen_two_stage_test;
template <typename T>
int Run(bool he, bool fidelity) {
  aa::base::TestContext test;
  const auto provider = aa::base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (bool upper : {false, true}) {
    for (bool row : {false, true}) {
      for (int band : {0, 1, 2}) {
        for (int work : {0, 1, 2}) {
          for (int n : {0, 1, 2, 7, 65, 193}) {
            aa::Case(test, provider, aa::Sample<T>(n, he, upper), row, band,
                     work, false, fidelity);
            ++cases;
          }
          for (int n : {1, 3, 7}) {
            aa::Case(test, provider, aa::Sample<T>(n, he, upper, true), row,
                     band, work, true, fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf("Two-stage checked cases=%d\n", cases);
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
