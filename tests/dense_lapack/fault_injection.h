#ifndef ASC_TESTS_DENSE_LAPACK_FAULT_INJECTION_H_
#define ASC_TESTS_DENSE_LAPACK_FAULT_INJECTION_H_
#include <cstdint>
namespace asc_lapack_test {
enum class InjectedFault : std::uint8_t {
  kNone,
  kNegativeInfo,
  kInvalidPivot,
  kExcessInfo
};
void SetInjectedFault(InjectedFault fault);
}  // namespace asc_lapack_test
#endif  // ASC_TESTS_DENSE_LAPACK_FAULT_INJECTION_H_
