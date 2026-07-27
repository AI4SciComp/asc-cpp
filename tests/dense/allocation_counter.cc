#include "allocation_counter.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

std::atomic<bool> g_count_allocations = false;
std::atomic<std::size_t> g_allocation_count = 0;

void RecordAllocation() noexcept {
  if (g_count_allocations.load(std::memory_order_relaxed)) {
    g_allocation_count.fetch_add(1, std::memory_order_relaxed);
  }
}

[[nodiscard]] void* Allocate(std::size_t size) {
  const std::size_t nonzero_size = size == 0 ? 1 : size;
  void* pointer = std::malloc(nonzero_size);
  if (pointer == nullptr) {
#if defined(__cpp_exceptions)
    throw std::bad_alloc();
#else
    std::abort();
#endif
  }
  RecordAllocation();
  return pointer;
}

[[nodiscard]] void* AllocateAligned(std::size_t size, std::size_t alignment) {
  const std::size_t nonzero_size = size == 0 ? 1 : size;
#if defined(_MSC_VER)
  void* pointer = _aligned_malloc(nonzero_size, alignment);
#else
  void* pointer = nullptr;
  if (posix_memalign(&pointer, alignment, nonzero_size) != 0) {
    pointer = nullptr;
  }
#endif
  if (pointer == nullptr) {
#if defined(__cpp_exceptions)
    throw std::bad_alloc();
#else
    std::abort();
#endif
  }
  RecordAllocation();
  return pointer;
}

void DeallocateAligned(void* pointer) noexcept {
#if defined(_MSC_VER)
  _aligned_free(pointer);
#else
  std::free(pointer);
#endif
}

}  // namespace

namespace asc_dense_test {

void ResetAllocationCount() noexcept {
  g_allocation_count.store(0, std::memory_order_relaxed);
}

void EnableAllocationCounting() noexcept {
  g_count_allocations.store(true, std::memory_order_relaxed);
}

void DisableAllocationCounting() noexcept {
  g_count_allocations.store(false, std::memory_order_relaxed);
}

std::size_t AllocationCount() noexcept {
  return g_allocation_count.load(std::memory_order_relaxed);
}

}  // namespace asc_dense_test

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

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
#if defined(__cpp_exceptions)
  try {
    return Allocate(size);
  } catch (...) {
    return nullptr;
  }
#else
  const std::size_t nonzero_size = size == 0 ? 1 : size;
  void* pointer = std::malloc(nonzero_size);
  if (pointer != nullptr) {
    RecordAllocation();
  }
  return pointer;
#endif
}

void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept {
  return ::operator new(size, tag);
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
  std::free(pointer);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
  return AllocateAligned(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
  return AllocateAligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* pointer, std::align_val_t) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept {
  DeallocateAligned(pointer);
}
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
  DeallocateAligned(pointer);
}

void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
#if defined(__cpp_exceptions)
  try {
    return AllocateAligned(size, static_cast<std::size_t>(alignment));
  } catch (...) {
    return nullptr;
  }
#else
#if defined(_MSC_VER)
  void* pointer = _aligned_malloc(size == 0 ? 1 : size,
                                  static_cast<std::size_t>(alignment));
#else
  void* pointer = nullptr;
  static_cast<void>(posix_memalign(
      &pointer, static_cast<std::size_t>(alignment), size == 0 ? 1 : size));
#endif
  if (pointer != nullptr) {
    RecordAllocation();
  }
  return pointer;
#endif
}

void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t& tag) noexcept {
  return ::operator new(size, alignment, tag);
}

void operator delete(void* pointer, std::align_val_t,
                     const std::nothrow_t&) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t,
                       const std::nothrow_t&) noexcept {
  DeallocateAligned(pointer);
}
