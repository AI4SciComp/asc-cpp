#ifndef ASC_TESTS_CORE_TEST_RESOURCES_H_
#define ASC_TESTS_CORE_TEST_RESOURCES_H_

#include <asc/core/memory_resource.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace asc::test {

class CountingMemoryResource : public MemoryResource {
 public:
  explicit CountingMemoryResource(
      MemoryResourcePtr delegate = GetHostMemoryResource(),
      MemorySpace space = MemorySpace::kHost)
      : delegate_(std::move(delegate)), space_(space) {}

  MemorySpace GetMemorySpace() const noexcept override { return space_; }

  std::string_view GetName() const noexcept override { return "counting"; }

  Result<void*> Allocate(std::size_t bytes,
                         std::size_t alignment) override {
    allocation_calls.fetch_add(1, std::memory_order_relaxed);
    allocated_bytes.fetch_add(bytes, std::memory_order_relaxed);
    last_alignment.store(alignment, std::memory_order_relaxed);
    return delegate_->Allocate(bytes, alignment);
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    deallocation_calls.fetch_add(1, std::memory_order_relaxed);
    deallocated_bytes.fetch_add(bytes, std::memory_order_relaxed);
    last_alignment.store(alignment, std::memory_order_relaxed);
    delegate_->Deallocate(pointer, bytes, alignment);
  }

  bool IsEqual(const MemoryResource& other) const noexcept override {
    return this == &other;
  }

  std::atomic<int> allocation_calls{0};
  std::atomic<int> deallocation_calls{0};
  std::atomic<std::size_t> allocated_bytes{0};
  std::atomic<std::size_t> deallocated_bytes{0};
  std::atomic<std::size_t> last_alignment{0};

 private:
  MemoryResourcePtr delegate_;
  MemorySpace space_;
};

class FailingMemoryResource final : public MemoryResource {
 public:
  MemorySpace GetMemorySpace() const noexcept override {
    return MemorySpace::kHost;
  }

  std::string_view GetName() const noexcept override { return "failing"; }

  Result<void*> Allocate(std::size_t, std::size_t) override {
    allocation_calls.fetch_add(1, std::memory_order_relaxed);
    return Status(StatusCode::kAllocationFailed,
                  "deterministic allocation failure");
  }

  void Deallocate(void*, std::size_t, std::size_t) noexcept override {
    deallocation_calls.fetch_add(1, std::memory_order_relaxed);
  }

  bool IsEqual(const MemoryResource& other) const noexcept override {
    return this == &other;
  }

  std::atomic<int> allocation_calls{0};
  std::atomic<int> deallocation_calls{0};
};

}  // namespace asc::test

#endif  // ASC_TESTS_CORE_TEST_RESOURCES_H_
