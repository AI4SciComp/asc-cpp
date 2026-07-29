#ifndef ASC_TESTS_SPARSE_TEST_RESOURCES_H_
#define ASC_TESTS_SPARSE_TEST_RESOURCES_H_

#include <cstddef>
#include <limits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc_sparse_test {

class TrackingMemoryResource final : public asc::MemoryResource {
 public:
  explicit TrackingMemoryResource(
      asc::MemorySpace reported_space = asc::MemorySpace::kHost) noexcept
      : reported_space_(reported_space) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return reported_space_;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_attempts_;
    if (allocation_attempts_ == failure_attempt_) {
      return asc::Status(asc::ErrorCode::kAllocation,
                         "Injected Sparse test allocation failure");
    }
    auto allocation = host_resource_.Allocate(bytes, alignment);
    if (allocation.ok()) {
      ++successful_allocations_;
      ++live_allocations_;
    }
    return allocation;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    host_resource_.Deallocate(pointer, bytes, alignment);
    ++deallocations_;
    if (live_allocations_ != 0) {
      --live_allocations_;
    }
  }

  void FailOnAllocation(std::size_t attempt) noexcept {
    failure_attempt_ = attempt;
  }

  void FailOnNextAllocation() noexcept {
    failure_attempt_ = allocation_attempts_ + 1;
  }

  void DisableFailure() noexcept {
    failure_attempt_ = std::numeric_limits<std::size_t>::max();
  }

  [[nodiscard]] std::size_t allocation_attempts() const noexcept {
    return allocation_attempts_;
  }

  [[nodiscard]] std::size_t successful_allocations() const noexcept {
    return successful_allocations_;
  }

  [[nodiscard]] std::size_t deallocations() const noexcept {
    return deallocations_;
  }

  [[nodiscard]] std::size_t live_allocations() const noexcept {
    return live_allocations_;
  }

 private:
  asc::HostMemoryResource host_resource_;
  asc::MemorySpace reported_space_;
  std::size_t allocation_attempts_ = 0;
  std::size_t successful_allocations_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t live_allocations_ = 0;
  std::size_t failure_attempt_ = std::numeric_limits<std::size_t>::max();
};

}  // namespace asc_sparse_test

#endif  // ASC_TESTS_SPARSE_TEST_RESOURCES_H_
