#include "allocation_probe.h"

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

bool g_probe_enabled = false;
std::size_t g_allocation_count = 0;

#if !defined(ASC_TEST_SANITIZER_OWNS_GLOBAL_ALLOCATOR)
void RecordAllocation() noexcept {
  if (g_probe_enabled) {
    ++g_allocation_count;
  }
}

void* AllocateNoThrow(std::size_t size) noexcept {
  RecordAllocation();
  return std::malloc(size == 0 ? 1 : size);
}

void* AllocateAlignedNoThrow(std::size_t size, std::size_t alignment) noexcept {
  RecordAllocation();
#if defined(_MSC_VER)
  const std::size_t allocation_size = size == 0 ? alignment : size;
  return _aligned_malloc(allocation_size, alignment);
#else
  const std::size_t remainder = size % alignment;
  std::size_t rounded_size = size == 0 ? alignment : size;
  if (size != 0 && remainder != 0) {
    const std::size_t increment = alignment - remainder;
    if (size > std::numeric_limits<std::size_t>::max() - increment) {
      return nullptr;
    }
    rounded_size += increment;
  }
  return std::aligned_alloc(alignment, rounded_size);
#endif
}

void* Allocate(std::size_t size) {
  if (void* pointer = AllocateNoThrow(size)) {
    return pointer;
  }
  std::abort();
}

void* AllocateAligned(std::size_t size, std::size_t alignment) {
  if (void* pointer = AllocateAlignedNoThrow(size, alignment)) {
    return pointer;
  }
  std::abort();
}

void DeallocateAligned(void* pointer) noexcept {
#if defined(_MSC_VER)
  _aligned_free(pointer);
#else
  std::free(pointer);
#endif
}
#endif

}  // namespace

#if !defined(ASC_TEST_SANITIZER_OWNS_GLOBAL_ALLOCATOR)
void* operator new(std::size_t size) { return Allocate(size); }
void* operator new[](std::size_t size) { return Allocate(size); }
void* operator new(std::size_t size, const std::nothrow_t& /*tag*/) noexcept {
  return AllocateNoThrow(size);
}
void* operator new[](std::size_t size, const std::nothrow_t& /*tag*/) noexcept {
  return AllocateNoThrow(size);
}
void* operator new(std::size_t size, std::align_val_t alignment) {
  return AllocateAligned(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
  return AllocateAligned(size, static_cast<std::size_t>(alignment));
}
void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t& /*tag*/) noexcept {
  return AllocateAlignedNoThrow(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t& /*tag*/) noexcept {
  return AllocateAlignedNoThrow(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t& /*tag*/) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t& /*tag*/) noexcept {
  std::free(pointer);
}
void operator delete(void* pointer, std::size_t /*size*/) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, std::size_t /*size*/) noexcept {
  std::free(pointer);
}
void operator delete(void* pointer, std::align_val_t /*alignment*/) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t /*alignment*/) noexcept {
  DeallocateAligned(pointer);
}
void operator delete(void* pointer, std::align_val_t /*alignment*/,
                     const std::nothrow_t& /*tag*/) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t /*alignment*/,
                       const std::nothrow_t& /*tag*/) noexcept {
  DeallocateAligned(pointer);
}
void operator delete(void* pointer, std::size_t /*size*/,
                     std::align_val_t /*alignment*/) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::size_t /*size*/,
                       std::align_val_t /*alignment*/) noexcept {
  DeallocateAligned(pointer);
}
#endif

namespace asc_dense_test {

AllocationProbe::AllocationProbe() noexcept : start_count_(g_allocation_count) {
  g_probe_enabled = true;
}

AllocationProbe::~AllocationProbe() { g_probe_enabled = false; }

std::size_t AllocationProbe::count() const noexcept {
  return g_allocation_count - start_count_;
}

}  // namespace asc_dense_test
