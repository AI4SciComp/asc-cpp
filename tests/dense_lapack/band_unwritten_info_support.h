#ifndef ASC_TESTS_DENSE_LAPACK_BAND_UNWRITTEN_INFO_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_UNWRITTEN_INFO_SUPPORT_H_

#include <cstddef>
#include <string_view>

namespace asc_band_unwritten_test {
void Reset(std::string_view routine, bool omit);
std::size_t Calls();
bool NativeSucceeded();
}  // namespace asc_band_unwritten_test

#endif  // ASC_TESTS_DENSE_LAPACK_BAND_UNWRITTEN_INFO_SUPPORT_H_
