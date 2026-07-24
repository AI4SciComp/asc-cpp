#include "allocation_counter.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {

std::atomic<bool> g_count_allocations{false};
std::atomic<std::size_t> g_allocation_count{0};

void* Allocate(std::size_t size) {
  asc::test::RecordAllocation();
  void* pointer = std::malloc(size == 0 ? 1 : size);
  if (pointer == nullptr) {
    std::abort();
  }
  return pointer;
}

}  // namespace

void* operator new(std::size_t size) { return Allocate(size); }

void* operator new[](std::size_t size) { return Allocate(size); }

void operator delete(void* pointer) noexcept { std::free(pointer); }

void operator delete[](void* pointer) noexcept { std::free(pointer); }

void operator delete(void* pointer, std::size_t) noexcept {
  std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
  std::free(pointer);
}

namespace asc::test {

void BeginAllocationCount() noexcept {
  g_allocation_count.store(0, std::memory_order_relaxed);
  g_count_allocations.store(true, std::memory_order_release);
}

std::size_t EndAllocationCount() noexcept {
  g_count_allocations.store(false, std::memory_order_release);
  return g_allocation_count.load(std::memory_order_relaxed);
}

void RecordAllocation() noexcept {
  if (g_count_allocations.load(std::memory_order_acquire)) {
    g_allocation_count.fetch_add(1, std::memory_order_relaxed);
  }
}

}  // namespace asc::test
