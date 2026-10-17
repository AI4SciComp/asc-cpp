#ifndef ASC_TESTS_DENSE_LAPACK_PACKED_TRIANGULAR_EXPERT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_PACKED_TRIANGULAR_EXPERT_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_packed_triangular_expert_test {
enum class Fault : std::uint8_t {
  kNone,
  kNoInfo,
  kNegativeInfo,
  kPositiveInfo,
  kLowZero,
  kLowOnes,
  kNegativeOutput,
  kNanOutput,
  kInfiniteOutput,
  kNoOutput
};
void Arm(std::size_t routine, Fault fault);
void Disarm();
[[nodiscard]] std::size_t Calls(std::size_t routine);
}  // namespace asc_packed_triangular_expert_test
#endif  // ASC_TESTS_DENSE_LAPACK_PACKED_TRIANGULAR_EXPERT_FAULTS_H_
