#ifndef ASC_TESTS_DENSE_COUNTING_MEMORY_RESOURCE_H_
#define ASC_TESTS_DENSE_COUNTING_MEMORY_RESOURCE_H_

#include <cstddef>
#include <optional>

#include "asc/core/memory.h"
#include "asc/core/status.h"

namespace asc_dense_test {

class CountingMemoryResource final : public asc::MemoryResource {
 public:
  explicit CountingMemoryResource(
      asc::MemorySpace space = asc::MemorySpace::kHost) noexcept
      : space_(space) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return space_;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    const std::size_t request = allocation_requests_++;
    if (fail_request_.has_value() && request == *fail_request_) {
      return asc::Status(asc::ErrorCode::kAllocation,
                         "Injected dense test allocation failure");
    }
    auto result = delegate_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++successful_allocations_;
      ++live_allocations_;
      allocated_bytes_ += bytes;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    ++deallocations_;
    if (live_allocations_ != 0) {
      --live_allocations_;
    }
    deallocated_bytes_ += bytes;
    delegate_.Deallocate(pointer, bytes, alignment);
  }

  void FailRequest(std::size_t request) noexcept { fail_request_ = request; }
  void DisableFailure() noexcept { fail_request_.reset(); }

  [[nodiscard]] std::size_t allocation_requests() const noexcept {
    return allocation_requests_;
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
  [[nodiscard]] std::size_t allocated_bytes() const noexcept {
    return allocated_bytes_;
  }
  [[nodiscard]] std::size_t deallocated_bytes() const noexcept {
    return deallocated_bytes_;
  }

 private:
  asc::MemorySpace space_;
  asc::HostMemoryResource delegate_;
  std::optional<std::size_t> fail_request_;
  std::size_t allocation_requests_ = 0;
  std::size_t successful_allocations_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t live_allocations_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t deallocated_bytes_ = 0;
};

}  // namespace asc_dense_test

#endif  // ASC_TESTS_DENSE_COUNTING_MEMORY_RESOURCE_H_
