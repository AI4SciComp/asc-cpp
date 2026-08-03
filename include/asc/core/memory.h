#ifndef ASC_CORE_MEMORY_H_
#define ASC_CORE_MEMORY_H_

/**
 * @file
 * @brief Public Core declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_core
 */

#include <cstddef>
#include <cstdint>

#include "asc/core/export.h"
#include "asc/core/result.h"

namespace asc {

/**
 * @brief Identifies host, pinned-host, or device memory placement.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
enum class MemorySpace : std::uint8_t {
  kHost = 0,        ///< Ordinary host-accessible memory.
  kPinnedHost = 1,  ///< Page-locked host memory suitable for CUDA transfers.
  kDevice = 2,      ///< Device-resident memory.
  kManaged = 3,     ///< Managed memory; no provider-free allocation promise.
};

/**
 * @brief Performs the public MemorySpaceName operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] space The space value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT const char* MemorySpaceName(MemorySpace space) noexcept;

/**
 * @brief Describes a non-owning immutable byte span and memory space.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ASC_CORE_EXPORT ConstMemoryView {
 public:
  /**
   * @brief Constructs a ConstMemoryView with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] data The data value required by this contract.
   * @param[in] size The size value required by this contract.
   * @param[in] space The space value required by this contract.
   * @ingroup asc_core
   */
  constexpr ConstMemoryView(const void* data, std::size_t size,
                            MemorySpace space) noexcept
      : data_(data), size_(size), space_(space) {}

  /**
   * @brief Returns the object's data contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The borrowed byte pointer; the view does not extend its lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] constexpr const void* data() const noexcept { return data_; }
  /**
   * @brief Returns the object's size contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  /**
   * @brief Returns the object's space contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] constexpr MemorySpace space() const noexcept { return space_; }
  /**
   * @brief Performs the public valid operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] constexpr bool valid() const noexcept {
    return size_ == 0 || data_ != nullptr;
  }

 private:
  const void* data_;
  std::size_t size_;
  MemorySpace space_;
};

/**
 * @brief Describes a non-owning mutable byte span and memory space.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ASC_CORE_EXPORT MutableMemoryView {
 public:
  /**
   * @brief Constructs a MutableMemoryView with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] data The data value required by this contract.
   * @param[in] size The size value required by this contract.
   * @param[in] space The space value required by this contract.
   * @ingroup asc_core
   */
  constexpr MutableMemoryView(void* data, std::size_t size,
                              MemorySpace space) noexcept
      : data_(data), size_(size), space_(space) {}

  /**
   * @brief Returns the object's data contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] constexpr void* data() const noexcept { return data_; }
  /**
   * @brief Returns the object's size contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  /**
   * @brief Returns the object's space contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] constexpr MemorySpace space() const noexcept { return space_; }
  /**
   * @brief Performs the public valid operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] constexpr bool valid() const noexcept {
    return size_ == 0 || data_ != nullptr;
  }
  /**
   * @brief Performs the public operator ConstMemoryView operation defined by
   * the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
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
/**
 * @brief Defines allocation, deallocation, and memory-space behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ASC_CORE_EXPORT MemoryResource {
 public:
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  virtual ~MemoryResource();

  /**
   * @brief Returns the object's space contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] virtual MemorySpace space() const noexcept = 0;
  /**
   * @brief Performs the public Allocate operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] bytes Requested byte count.
   * @param[in] alignment Power-of-two allocation alignment.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  virtual Result<void*> Allocate(std::size_t bytes, std::size_t alignment) = 0;
  /**
   * @brief Performs the public Deallocate operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] pointer The pointer value required by this contract.
   * @param[in] bytes Requested byte count.
   * @param[in] alignment Power-of-two allocation alignment.
   * @ingroup asc_core
   */
  virtual void Deallocate(void* pointer, std::size_t bytes,
                          std::size_t alignment) noexcept = 0;
};

/**
 * @brief Allocates aligned host memory.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ASC_CORE_EXPORT HostMemoryResource final : public MemoryResource {
 public:
  /**
   * @brief Returns the object's space contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] MemorySpace space() const noexcept override {
    return MemorySpace::kHost;
  }
  /**
   * @brief Performs the public Allocate operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] bytes Requested byte count.
   * @param[in] alignment Power-of-two allocation alignment.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  Result<void*> Allocate(std::size_t bytes, std::size_t alignment) override;
  /**
   * @brief Performs the public Deallocate operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] pointer The pointer value required by this contract.
   * @param[in] bytes Requested byte count.
   * @param[in] alignment Power-of-two allocation alignment.
   * @ingroup asc_core
   */
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override;
};

/**
 * @brief Owns one allocation obtained from a MemoryResource.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ASC_CORE_EXPORT Buffer {
 public:
  /**
   * @brief Performs the public Allocate operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] resource Allocator that must outlive storage allocated from it.
   * @param[in] bytes Requested byte count.
   * @param[in] alignment Power-of-two allocation alignment.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  static Result<Buffer> Allocate(
      MemoryResource& resource, std::size_t bytes,
      std::size_t alignment = alignof(std::max_align_t));

  /**
   * @brief Constructs a Buffer with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  Buffer(const Buffer&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  Buffer& operator=(const Buffer&) = delete;
  /**
   * @brief Constructs a Buffer with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] other The other value required by this contract.
   * @ingroup asc_core
   */
  Buffer(Buffer&& other) noexcept;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] other The other value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  Buffer& operator=(Buffer&& other) noexcept;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ~Buffer();

  /**
   * @brief Reports whether the documented valid condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool valid() const noexcept { return resource_ != nullptr; }
  /**
   * @brief Returns the object's data contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] void* data() noexcept { return data_; }
  /**
   * @brief Returns the object's data contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] const void* data() const noexcept { return data_; }
  /**
   * @brief Returns the object's size contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  /**
   * @brief Returns the object's alignment contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }
  /**
   * @brief Returns the object's space contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] Result<MemorySpace> space() const;
  /**
   * @brief Performs the public mutable_view operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] Result<MutableMemoryView> mutable_view();
  /**
   * @brief Performs the public const_view operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] Result<ConstMemoryView> const_view() const;

  // Releases the allocation exactly once. The resource must still be alive.
  /**
   * @brief Performs the reset state transition defined by this Core object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
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
