#ifndef ASC_BENCHMARKS_RANDOM_STORAGE_ALLOCATION_PROBE_H_
#define ASC_BENCHMARKS_RANDOM_STORAGE_ALLOCATION_PROBE_H_

#include <cstddef>

#include "../../tests/allocation_observation.h"

namespace asc_random_storage_benchmark {

class AllocationProbe {
 public:
  AllocationProbe() noexcept;
  AllocationProbe(const AllocationProbe&) = delete;
  AllocationProbe& operator=(const AllocationProbe&) = delete;
  AllocationProbe(AllocationProbe&&) = delete;
  AllocationProbe& operator=(AllocationProbe&&) = delete;
  ~AllocationProbe();

  [[nodiscard]] std::size_t count() const noexcept;

 private:
  std::size_t start_count_;
};

}  // namespace asc_random_storage_benchmark

#endif  // ASC_BENCHMARKS_RANDOM_STORAGE_ALLOCATION_PROBE_H_
