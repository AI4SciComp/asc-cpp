#ifndef ASC_CPP_TESTS_DENSE_LAPACK_INSTALLED_LU_NORMAL_RETURN_GUARD_H_
#define ASC_CPP_TESTS_DENSE_LAPACK_INSTALLED_LU_NORMAL_RETURN_GUARD_H_

#include <cstdio>
#include <cstdlib>

// This is an example verification helper, not an installed ASC declaration.
/// @cond ASC_EXAMPLE_INTERNAL
namespace asc_lapack_test {

// Create exactly once, as the first local in main. Some Fortran STOP paths call
// exit(0), which skips automatic destructors and otherwise looks like a pass to
// CTest. Normal return destroys this guard before the registered exit callback.
// This helper belongs only to tests; it never replaces a provider error
// handler.
class NormalReturnGuard {
 public:
  NormalReturnGuard() {
    if (std::atexit(CheckReturned) != 0) {
      std::_Exit(92);
    }
  }

  NormalReturnGuard(const NormalReturnGuard&) = delete;
  NormalReturnGuard& operator=(const NormalReturnGuard&) = delete;
  NormalReturnGuard(NormalReturnGuard&&) = delete;
  NormalReturnGuard& operator=(NormalReturnGuard&&) = delete;

  ~NormalReturnGuard() { returned = true; }

 private:
  static void CheckReturned() {
    if (!returned) {
      std::fputs("LAPACK test did not return from main\n", stderr);
      std::_Exit(93);
    }
  }

  inline static bool returned = false;
};

}  // namespace asc_lapack_test
/// @endcond

#endif  // ASC_CPP_TESTS_DENSE_LAPACK_INSTALLED_LU_NORMAL_RETURN_GUARD_H_
