#include <cstdlib>
#include <string_view>

#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapack.h>
#include <lapacke_config.h>

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 1;
  }
  const std::string_view mode(argv[1]);
  if (mode == "return") {
    return 0;
  }
  if (mode == "exit") {
    // This single-threaded control deliberately bypasses automatic destruction.
    std::exit(0);  // NOLINT(concurrency-mt-unsafe)
  }
  if (mode != "provider_stop") {
    return 2;
  }
  // Deliberately enter the pinned provider's unmodified XERBLA/STOP path.
  // Calling the checked ASC adapter here would reject before foreign entry.
  const lapack_int invalid_rows = -1;
  const lapack_int one = 1;
  double matrix = 1;
  lapack_int pivot = 0;
  lapack_int info = 0;
  LAPACK_dgetrf(&invalid_rows, &one, &matrix, &one, &pivot, &info);
  return 3;
}
