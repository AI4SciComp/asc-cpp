#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "indefinite_aasen_driver_native.h"
#include "installed_lu/normal_return_guard.h"

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view triangle(argv[1]);
  const std::string_view columns(argv[2]);
  if ((triangle != "U" && triangle != "L") ||
      (columns != "0" && columns != "3")) {
    return 2;
  }
  // CSYSV_AA documents and initially accepts LWORK=max(2*N,3*N-2)=0.
  // Its TRF_AA dependency requires one even for N=0. NormalReturnGuard
  // makes a nested Fortran STOP(exit=0) a real failed test process.
  std::complex<float> a{-731, 7};
  std::array<std::complex<float>, 3> b{};
  std::complex<float> work{-733, 11};
  lapack_int pivot = -737;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  std::printf("CSYSV_AA documented empty call triangle=%c nrhs=%s LWORK=0\n",
              triangle.front(), argv[2]);
  std::fflush(stdout);
  asc_aasen_driver_test::Native(false, triangle.front(), 0,
                                columns == "0" ? 0 : 3, &a, 1, &pivot, b.data(),
                                1, &work, 0, info);
  std::printf("CSYSV_AA empty returned INFO=%lld WORK=(%g,%g)\n",
              static_cast<long long>(info), static_cast<double>(work.real()),
              static_cast<double>(work.imag()));
  return info == 0 && a == std::complex<float>{-731, 7} && pivot == -737 &&
                 b == std::array<std::complex<float>, 3>{}
             ? 0
             : 1;
}
