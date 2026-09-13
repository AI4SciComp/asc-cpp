#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INFO_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INFO_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_indefinite_rook_info_test {
enum class Routine : std::uint8_t { kTrf, kTf2, kTrs };
enum class Fault : std::uint8_t { kPass, kOmitInfo, kWrite32BitZero };
void SetFault(Routine routine, Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
}  // namespace asc_indefinite_rook_info_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INFO_FAULTS_H_
