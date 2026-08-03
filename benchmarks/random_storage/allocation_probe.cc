#include "allocation_probe.h"

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

bool g_enabled = false;
std::size_t g_count = 0;

#if !defined(ASC_TEST_SANITIZER_OWNS_GLOBAL_ALLOCATOR)
void Record() noexcept {
  if (g_enabled) {
    ++g_count;
  }
}

void* AllocateNoThrow(std::size_t size) noexcept {
  Record();
  return std::malloc(size == 0 ? 1 : size);
}

void* AllocateAlignedNoThrow(std::size_t size, std::size_t alignment) noexcept {
  Record();
#if defined(_MSC_VER)
  return _aligned_malloc(size == 0 ? alignment : size, alignment);
#else
  std::size_t rounded = size == 0 ? alignment : size;
  const std::size_t remainder = rounded % alignment;
  if (remainder != 0) {
    const std::size_t increment = alignment - remainder;
    if (rounded > std::numeric_limits<std::size_t>::max() - increment) {
      return nullptr;
    }
    rounded += increment;
  }
  return std::aligned_alloc(alignment, rounded);
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
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  return AllocateNoThrow(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
  return AllocateNoThrow(size);
}
void* operator new(std::size_t size, std::align_val_t alignment) {
  return AllocateAligned(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
  return AllocateAligned(size, static_cast<std::size_t>(alignment));
}
void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
  return AllocateAlignedNoThrow(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
  return AllocateAlignedNoThrow(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
  std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
  std::free(pointer);
}
void operator delete(void* pointer, std::align_val_t) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept {
  DeallocateAligned(pointer);
}
void operator delete(void* pointer, std::align_val_t,
                     const std::nothrow_t&) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t,
                       const std::nothrow_t&) noexcept {
  DeallocateAligned(pointer);
}
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
  DeallocateAligned(pointer);
}
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
  DeallocateAligned(pointer);
}
#endif

namespace asc_random_storage_benchmark {

AllocationProbe::AllocationProbe() noexcept : start_count_(g_count) {
  g_enabled = true;
}

AllocationProbe::~AllocationProbe() { g_enabled = false; }

std::size_t AllocationProbe::count() const noexcept {
  return g_count - start_count_;
}

}  // namespace asc_random_storage_benchmark
