#ifndef ASC_TESTS_DENSE_CUDA_COUNTING_RESOURCE_H_
#define ASC_TESTS_DENSE_CUDA_COUNTING_RESOURCE_H_

#include <cstddef>

#include "asc/core/memory.h"
#include "asc/core/result.h"

namespace asc_dense_cuda_test {

class CountingResource final : public asc::MemoryResource {
 public:
  explicit CountingResource(asc::MemoryResource& upstream) noexcept
      : upstream_(upstream) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return upstream_.space();
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_calls_;
    auto result = upstream_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++live_allocations_;
      allocated_bytes_ += bytes;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    ++deallocation_calls_;
    if (pointer != nullptr) {
      --live_allocations_;
      deallocated_bytes_ += bytes;
    }
    upstream_.Deallocate(pointer, bytes, alignment);
  }

  [[nodiscard]] std::size_t allocation_calls() const noexcept {
    return allocation_calls_;
  }
  [[nodiscard]] std::size_t deallocation_calls() const noexcept {
    return deallocation_calls_;
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
  asc::MemoryResource& upstream_;
  std::size_t allocation_calls_ = 0;
  std::size_t deallocation_calls_ = 0;
  std::size_t live_allocations_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t deallocated_bytes_ = 0;
};

}  // namespace asc_dense_cuda_test

#endif  // ASC_TESTS_DENSE_CUDA_COUNTING_RESOURCE_H_
