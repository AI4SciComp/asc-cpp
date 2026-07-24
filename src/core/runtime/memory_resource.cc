// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include <asc/core/memory_resource.h>

#include <new>
#include <string_view>

namespace asc {
namespace {

bool IsValidAlignment(std::size_t alignment) noexcept {
  return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

class HostMemoryResource final : public MemoryResource {
 public:
  MemorySpace GetMemorySpace() const noexcept override {
    return MemorySpace::kHost;
  }

  std::string_view GetName() const noexcept override { return "host"; }

  Result<void*> Allocate(std::size_t bytes,
                         std::size_t alignment) override {
    if (!IsValidAlignment(alignment)) {
      return Status(
          StatusCode::kInvalidArgument,
          "Memory-resource alignment must be a non-zero power of two");
    }
    if (bytes == 0) {
      return static_cast<void*>(nullptr);
    }

    void* pointer = nullptr;
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
      pointer = ::operator new(bytes, std::align_val_t(alignment),
                               std::nothrow);
    } else {
      pointer = ::operator new(bytes, std::nothrow);
    }
#else
    pointer =
        ::operator new(bytes, std::align_val_t(alignment), std::nothrow);
#endif
    if (pointer == nullptr) {
      return Status(StatusCode::kAllocationFailed,
                    "Host memory allocation failed");
    }
    return pointer;
  }

  void Deallocate(void* pointer, std::size_t,
                  std::size_t alignment) noexcept override {
    if (pointer == nullptr) {
      return;
    }
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
      ::operator delete(pointer, std::align_val_t(alignment));
    } else {
      ::operator delete(pointer);
    }
#else
    ::operator delete(pointer, std::align_val_t(alignment));
#endif
  }

  bool IsEqual(const MemoryResource& other) const noexcept override {
    return this == &other;
  }
};

}  // namespace

MemoryResourcePtr GetHostMemoryResource() {
  static MemoryResourcePtr resource = std::make_shared<HostMemoryResource>();
  return resource;
}

}  // namespace asc
