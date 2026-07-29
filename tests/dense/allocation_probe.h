#ifndef ASC_TESTS_DENSE_ALLOCATION_PROBE_H_
#define ASC_TESTS_DENSE_ALLOCATION_PROBE_H_

#include <cstddef>

namespace asc_dense_test {

class AllocationProbe {
 public:
  AllocationProbe() noexcept;
  AllocationProbe(const AllocationProbe&) = delete;
  AllocationProbe& operator=(const AllocationProbe&) = delete;
  ~AllocationProbe();

  [[nodiscard]] std::size_t count() const noexcept;

 private:
  std::size_t start_count_;
};

}  // namespace asc_dense_test

#endif  // ASC_TESTS_DENSE_ALLOCATION_PROBE_H_
