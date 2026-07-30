#ifndef ASC_CORE_MEMORY_H_
#define ASC_CORE_MEMORY_H_

#include <cstddef>
#include <cstdint>

#include "asc/core/export.h"
#include "asc/core/result.h"

namespace asc {

enum class MemorySpace : std::uint8_t {
  kHost = 0,
  kPinnedHost = 1,
  kDevice = 2,
  kManaged = 3,
};

ASC_CORE_EXPORT const char* MemorySpaceName(MemorySpace space) noexcept;

class ASC_CORE_EXPORT ConstMemoryView {
 public:
  constexpr ConstMemoryView(const void* data, std::size_t size,
                            MemorySpace space) noexcept
      : data_(data), size_(size), space_(space) {}

  [[nodiscard]] constexpr const void* data() const noexcept { return data_; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr MemorySpace space() const noexcept { return space_; }
  [[nodiscard]] constexpr bool valid() const noexcept {
    return size_ == 0 || data_ != nullptr;
  }

 private:
  const void* data_;
  std::size_t size_;
  MemorySpace space_;
};

class ASC_CORE_EXPORT MutableMemoryView {
 public:
  constexpr MutableMemoryView(void* data, std::size_t size,
                              MemorySpace space) noexcept
      : data_(data), size_(size), space_(space) {}

  [[nodiscard]] constexpr void* data() const noexcept { return data_; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr MemorySpace space() const noexcept { return space_; }
  [[nodiscard]] constexpr bool valid() const noexcept {
    return size_ == 0 || data_ != nullptr;
  }
  // Mutable-to-const view conversion is intentionally implicit.
  // NOLINTNEXTLINE(google-explicit-constructor)
  [[nodiscard]] constexpr operator ConstMemoryView() const noexcept {
    return {data_, size_, space_};
  }

 private:
  void* data_;
  std::size_t size_;
  MemorySpace space_;
};

// A MemoryResource instance must outlive every Buffer allocated from it.
class ASC_CORE_EXPORT MemoryResource {
 public:
  virtual ~MemoryResource();

  [[nodiscard]] virtual MemorySpace space() const noexcept = 0;
  virtual Result<void*> Allocate(std::size_t bytes, std::size_t alignment) = 0;
  virtual void Deallocate(void* pointer, std::size_t bytes,
                          std::size_t alignment) noexcept = 0;
};

class ASC_CORE_EXPORT HostMemoryResource final : public MemoryResource {
 public:
  [[nodiscard]] MemorySpace space() const noexcept override {
    return MemorySpace::kHost;
  }
  Result<void*> Allocate(std::size_t bytes, std::size_t alignment) override;
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override;
};

class ASC_CORE_EXPORT Buffer {
 public:
  static Result<Buffer> Allocate(
      MemoryResource& resource, std::size_t bytes,
      std::size_t alignment = alignof(std::max_align_t));

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  Buffer(Buffer&& other) noexcept;
  Buffer& operator=(Buffer&& other) noexcept;
  ~Buffer();

  [[nodiscard]] bool valid() const noexcept { return resource_ != nullptr; }
  [[nodiscard]] void* data() noexcept { return data_; }
  [[nodiscard]] const void* data() const noexcept { return data_; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }
  [[nodiscard]] Result<MemorySpace> space() const;
  [[nodiscard]] Result<MutableMemoryView> mutable_view();
  [[nodiscard]] Result<ConstMemoryView> const_view() const;

  // Releases the allocation exactly once. The resource must still be alive.
  void Reset() noexcept;

 private:
  Buffer(MemoryResource* resource, void* data, std::size_t size,
         std::size_t alignment) noexcept;

  MemoryResource* resource_ = nullptr;
  void* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t alignment_ = 0;
};

}  // namespace asc

#endif  // ASC_CORE_MEMORY_H_
