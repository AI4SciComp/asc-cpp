#include <complex.h>
#include <string.h>

/* Test-only representation bridge. memcpy avoids imposing a C effective type
 * on live C++ objects. The Fortran and numerical probes test typed foreign use.
 */
int AscLapackCComplexProbe(void* single_values, size_t single_bytes,
                           void* double_values, size_t double_bytes) {
  float complex single[2];
  double complex wide[2];
  if (single_values == NULL || double_values == NULL ||
      single_bytes != sizeof(single) || double_bytes != sizeof(wide)) {
    return 1;
  }
  /* Exact two-object capacities are checked above. Optional Annex K memcpy_s
   * is not supplied by this audited GNU C library. */
  // NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  memcpy(single, single_values, sizeof(single));
  memcpy(wide, double_values, sizeof(wide));
  if (crealf(single[0]) != 2 || cimagf(single[0]) != -3 ||
      crealf(single[1]) != -5 || cimagf(single[1]) != 7 ||
      creal(wide[0]) != 11 || cimag(wide[0]) != -13 || creal(wide[1]) != -17 ||
      cimag(wide[1]) != 19) {
    return 1;
  }
  single[0] = conjf(single[0]) + 1;
  single[1] *= 2;
  wide[0] = conj(wide[0]) + 1;
  wide[1] *= 2;
  memcpy(single_values, single, sizeof(single));
  memcpy(double_values, wide, sizeof(wide));
  // NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  return 0;
}
