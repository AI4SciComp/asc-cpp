#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_ORIGIN_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_ORIGIN_H_

#include <complex>
#include <string_view>
#include <type_traits>

namespace asc::internal_indefinite_rook {
// Actual driver origins remain distinct from the factorization entry points.
template <typename T>
std::string_view DriverName(bool hermitian) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssysv_rook";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsysv_rook";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "chesv_rook" : "csysv_rook";
  } else {
    return hermitian ? "zhesv_rook" : "zsysv_rook";
  }
}
}  // namespace asc::internal_indefinite_rook

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_ORIGIN_H_
