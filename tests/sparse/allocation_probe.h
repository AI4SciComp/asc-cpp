#ifndef ASC_TESTS_SPARSE_ALLOCATION_PROBE_H_
#define ASC_TESTS_SPARSE_ALLOCATION_PROBE_H_

#include <cstddef>

namespace asc_sparse_test {

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

}  // namespace asc_sparse_test

#endif  // ASC_TESTS_SPARSE_ALLOCATION_PROBE_H_
