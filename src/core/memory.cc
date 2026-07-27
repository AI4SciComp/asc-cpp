#include "asc/core/memory.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

#include "asc/core/status.h"

namespace asc {

namespace internal_core_memory {

bool IsPowerOfTwo(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1U)) == 0;
}

std::size_t EffectiveAlignment(std::size_t alignment) noexcept {
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
  constexpr std::size_t kDefaultAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
  constexpr std::size_t kDefaultAlignment = alignof(std::max_align_t);
#endif
  return std::max(alignment, kDefaultAlignment);
}

}  // namespace internal_core_memory

const char* MemorySpaceName(MemorySpace space) noexcept {
  switch (space) {
    case MemorySpace::kHost:
      return "host";
    case MemorySpace::kPinnedHost:
      return "pinned-host";
    case MemorySpace::kDevice:
      return "device";
    case MemorySpace::kManaged:
      return "managed";
  }
  return "unknown";
}

MemoryResource::~MemoryResource() = default;

Result<void*> HostMemoryResource::Allocate(std::size_t bytes,
                                           std::size_t alignment) {
  if (!internal_core_memory::IsPowerOfTwo(alignment)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Allocation alignment must be a nonzero power of two");
  }
  if (bytes == 0) {
    return static_cast<void*>(nullptr);
  }

  const std::size_t effective_alignment =
      internal_core_memory::EffectiveAlignment(alignment);
  void* pointer = ::operator new(bytes, std::align_val_t(effective_alignment),
                                 std::nothrow);
  if (pointer == nullptr) {
    return Status(ErrorCode::kAllocation, "Host allocation failed");
  }
  return pointer;
}

void HostMemoryResource::Deallocate(void* pointer, std::size_t bytes,
                                    std::size_t alignment) noexcept {
  static_cast<void>(bytes);
  if (pointer == nullptr) {
    return;
  }
  const std::size_t effective_alignment =
      internal_core_memory::EffectiveAlignment(alignment);
  ::operator delete(pointer, std::align_val_t(effective_alignment));
}

Result<Buffer> Buffer::Allocate(MemoryResource& resource, std::size_t bytes,
                                std::size_t alignment) {
  if (!internal_core_memory::IsPowerOfTwo(alignment)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Buffer alignment must be a nonzero power of two");
  }
  auto allocation = resource.Allocate(bytes, alignment);
  if (!allocation.ok()) {
    return allocation.status();
  }
  if (bytes != 0 && *allocation == nullptr) {
    return Status(ErrorCode::kAllocation,
                  "MemoryResource returned null for a nonzero allocation");
  }
  if (bytes == 0 && *allocation != nullptr) {
    resource.Deallocate(*allocation, bytes, alignment);
    return Status(ErrorCode::kInternal,
                  "MemoryResource allocated storage for a zero-byte request");
  }
  if (*allocation != nullptr &&
      reinterpret_cast<std::uintptr_t>(*allocation) % alignment != 0) {
    resource.Deallocate(*allocation, bytes, alignment);
    return Status(ErrorCode::kAllocation,
                  "MemoryResource returned a misaligned allocation");
  }
  return Buffer(&resource, *allocation, bytes, alignment);
}

Buffer::Buffer(MemoryResource* resource, void* data, std::size_t size,
               std::size_t alignment) noexcept
    : resource_(resource), data_(data), size_(size), alignment_(alignment) {}

Buffer::Buffer(Buffer&& other) noexcept
    : resource_(std::exchange(other.resource_, nullptr)),
      data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      alignment_(std::exchange(other.alignment_, 0)) {}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
  if (this != &other) {
    Reset();
    resource_ = std::exchange(other.resource_, nullptr);
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    alignment_ = std::exchange(other.alignment_, 0);
  }
  return *this;
}

Buffer::~Buffer() { Reset(); }

Result<MemorySpace> Buffer::space() const {
  if (!valid()) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from Buffer has no MemoryResource");
  }
  return resource_->space();
}

Result<MutableMemoryView> Buffer::mutable_view() {
  auto memory_space = space();
  if (!memory_space.ok()) {
    return memory_space.status();
  }
  return MutableMemoryView(data_, size_, *memory_space);
}

Result<ConstMemoryView> Buffer::const_view() const {
  auto memory_space = space();
  if (!memory_space.ok()) {
    return memory_space.status();
  }
  return ConstMemoryView(data_, size_, *memory_space);
}

void Buffer::Reset() noexcept {
  if (resource_ != nullptr && data_ != nullptr) {
    resource_->Deallocate(data_, size_, alignment_);
  }
  resource_ = nullptr;
  data_ = nullptr;
  size_ = 0;
  alignment_ = 0;
}

}  // namespace asc
