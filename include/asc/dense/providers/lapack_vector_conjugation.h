#ifndef ASC_DENSE_PROVIDERS_LAPACK_VECTOR_CONJUGATION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_VECTOR_CONJUGATION_H_
/** @file
 * @brief Checked in-place Reference conjugation with signed or zero spacing.
 *
 * Storage is a unit-contiguous physical complex buffer. For n>0 its length is
 * exactly 1+(n-1)*abs(increment); for n=0 it is empty. Positive increments
 * start at storage[0], negative increments at the last physical element, and
 * zero increment conjugates storage[0] n times. Unselected gaps are untouched.
 * Storage is borrowed, live, and on host or admitted pinned-host memory.
 *
 * Native loop counts, starting offsets and final cursor updates must fit the
 * selected integer ABI. Empty calls are local and leave INFO absent. A single
 * element uses native increment one, so its original increment is immaterial.
 * There is no workspace, query, allocation, transfer or synchronization.
 *
 * The provider and report must be disjoint from each other and the reachable
 * storage span, including gaps. Metadata aliasing rejects before report reset;
 * other preflight errors reset the report and preserve every numeric element.
 * After entry the provider modifies selected elements in place. There is no
 * native INFO or return-status channel: successful completion records normal
 * return, not detection of arbitrary omitted or incorrect provider writes.
 * Nonfinite components are valid transformation inputs and do not by themselves
 * produce a numerical warning. Floating-point control settings are unchanged.
 * Concurrent calls share only immutable provider state and use disjoint mutable
 * storage and reports.
 */
#include <complex>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {
/** @brief Conjugates n selected complex<float> entries through CLACGV.
 * @param provider Explicit checked serial Reference provider.
 * @param n Nonnegative number of conjugations, including repeated zero spacing.
 * @param increment Signed physical spacing; see the file contract for origin.
 * @param storage Mutable physical buffer, length as above and view increment
 * one.
 * @param report Records actual entry, absent native INFO and completion
 * validity.
 * @return Success on local empty completion or normal native return; otherwise
 * a structural, access or integer-overflow error before numeric mutation.
 * @post Selected entries are conjugated in place; every gap is preserved. With
 * zero spacing the single entry changes only when n is odd.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lacgv(const ReferenceLapackProvider& provider, extent_t n, stride_t increment,
      DenseBlasVectorView<std::complex<float>> storage, LapackReport& report);
/** @brief Conjugates n selected complex<double> entries through ZLACGV.
 * @param provider Explicit checked serial Reference provider.
 * @param n Nonnegative number of conjugations, including repeated zero spacing.
 * @param increment Signed physical spacing; see the file contract for origin.
 * @param storage Mutable physical buffer, length as above and view increment
 * one.
 * @param report Records actual entry, absent native INFO and completion
 * validity.
 * @return Success on local empty completion or normal native return; otherwise
 * a structural, access or integer-overflow error before numeric mutation.
 * @post Selected entries are conjugated in place; every gap is preserved. With
 * zero spacing the single entry changes only when n is odd.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lacgv(const ReferenceLapackProvider& provider, extent_t n, stride_t increment,
      DenseBlasVectorView<std::complex<double>> storage, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_VECTOR_CONJUGATION_H_
