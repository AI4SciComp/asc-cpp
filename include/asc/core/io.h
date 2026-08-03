#ifndef ASC_CORE_IO_H_
#define ASC_CORE_IO_H_

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

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "asc/core/export.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

/**
 * @brief Provides a synchronous sequence of bytes.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ASC_CORE_EXPORT ByteSource {
 public:
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  virtual ~ByteSource();

  // Returns zero at end of stream. Implementations are synchronous and do not
  // use zero to report a temporary would-block state. A zero-sized request
  // succeeds without accessing its pointer.
  /**
   * @brief Reads bytes synchronously with explicit end-of-file and I/O
   * failures.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[out] destination Destination storage with the required size and
   * accessibility.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  virtual Result<std::size_t> ReadSome(std::span<std::byte> destination) = 0;
};

/**
 * @brief Receives a synchronous sequence of bytes.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ASC_CORE_EXPORT ByteSink {
 public:
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  virtual ~ByteSink();

  // A zero-sized request succeeds without accessing its pointer.
  /**
   * @brief Writes bytes synchronously with explicit progress and I/O failures.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] source Input source, valid and accessible for the operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  virtual Result<std::size_t> WriteSome(std::span<const std::byte> source) = 0;
};

/**
 * @brief Reads bytes synchronously with explicit end-of-file and I/O failures.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] source Input source, valid and accessible for the operation.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @return OK on success; otherwise a stable failure category with optional
 * diagnostics.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT Status ReadExact(ByteSource& source,
                                 std::span<std::byte> destination);
/**
 * @brief Writes bytes synchronously with explicit progress and I/O failures.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] sink The sink value required by this contract.
 * @param[in] source Input source, valid and accessible for the operation.
 * @return OK on success; otherwise a stable failure category with optional
 * diagnostics.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT Status WriteAll(ByteSink& sink,
                                std::span<const std::byte> source);

/**
 * @brief Owns a synchronous readable or writable operating-system file handle.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ASC_CORE_EXPORT File final : public ByteSource, public ByteSink {
 public:
  /**
   * @brief Performs the public OpenRead operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] path Filesystem path interpreted by the synchronous I/O
   * operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  static Result<File> OpenRead(const std::filesystem::path& path);
  /**
   * @brief Performs the public OpenWrite operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] path Filesystem path interpreted by the synchronous I/O
   * operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  static Result<File> OpenWrite(const std::filesystem::path& path);

  /**
   * @brief Constructs a File with the documented ownership and validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  File(const File&) = delete;
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
  File& operator=(const File&) = delete;
  /**
   * @brief Constructs a File with the documented ownership and validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] other The other value required by this contract.
   * @ingroup asc_core
   */
  File(File&& other) noexcept;
  // The destination must be closed before move assignment so an earlier close
  // failure cannot be discarded.
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
  File& operator=(File&& other) noexcept;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ~File() override;

  /**
   * @brief Reports whether the documented is_open condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }
  /**
   * @brief Reports whether the documented readable condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool readable() const noexcept { return readable_; }
  /**
   * @brief Reports whether the documented writable condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool writable() const noexcept { return writable_; }

  /**
   * @brief Reads bytes synchronously with explicit end-of-file and I/O
   * failures.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[out] destination Destination storage with the required size and
   * accessibility.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  Result<std::size_t> ReadSome(std::span<std::byte> destination) override;
  /**
   * @brief Writes bytes synchronously with explicit progress and I/O failures.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] source Input source, valid and accessible for the operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  Result<std::size_t> WriteSome(std::span<const std::byte> source) override;
  /**
   * @brief Performs the flush state transition defined by this Core object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  Status Flush();
  /**
   * @brief Performs the close state transition defined by this Core object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  Status Close() noexcept;

 private:
  File(void* handle, bool readable, bool writable) noexcept;

  void* handle_ = nullptr;
  bool readable_ = false;
  bool writable_ = false;
};

/**
 * @brief Reads bytes synchronously with explicit end-of-file and I/O failures.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] path Filesystem path interpreted by the synchronous I/O operation.
 * @param[in] maximum_bytes The maximum bytes value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT Result<std::string> ReadTextFile(
    const std::filesystem::path& path, std::size_t maximum_bytes);
/**
 * @brief Writes bytes synchronously with explicit progress and I/O failures.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] path Filesystem path interpreted by the synchronous I/O operation.
 * @param[in] text The text value required by this contract.
 * @return OK on success; otherwise a stable failure category with optional
 * diagnostics.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT Status WriteTextFile(const std::filesystem::path& path,
                                     std::string_view text);

/**
 * @brief Defines the public LittleEndianScalar concept contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
template <typename T>
concept LittleEndianScalar =
    (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool> &&
     (sizeof(T) == sizeof(std::uint8_t) || sizeof(T) == sizeof(std::uint16_t) ||
      sizeof(T) == sizeof(std::uint32_t) ||
      sizeof(T) == sizeof(std::uint64_t))) ||
    (std::floating_point<T> && std::numeric_limits<T>::is_iec559 &&
     (sizeof(T) == sizeof(std::uint32_t) ||
      sizeof(T) == sizeof(std::uint64_t)));

namespace internal_core_io {

template <LittleEndianScalar T>
using UnsignedScalar = std::conditional_t<
    sizeof(T) == sizeof(std::uint8_t), std::uint8_t,
    std::conditional_t<sizeof(T) == sizeof(std::uint16_t), std::uint16_t,
                       std::conditional_t<sizeof(T) == sizeof(std::uint32_t),
                                          std::uint32_t, std::uint64_t>>>;

template <LittleEndianScalar T>
UnsignedScalar<T> ToUnsignedBits(T value) noexcept {
  using Unsigned = UnsignedScalar<T>;
  if constexpr (std::floating_point<T> || std::is_signed_v<T>) {
    return std::bit_cast<Unsigned>(value);
  } else {
    return static_cast<Unsigned>(value);
  }
}

template <LittleEndianScalar T>
T FromUnsignedBits(UnsignedScalar<T> bits) noexcept {
  if constexpr (std::floating_point<T> || std::is_signed_v<T>) {
    return std::bit_cast<T>(bits);
  } else {
    return static_cast<T>(bits);
  }
}

}  // namespace internal_core_io

/**
 * @brief Performs the public EncodeLittleEndian operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] value Value read or written by the operation.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @return OK on success; otherwise a stable failure category with optional
 * diagnostics.
 * @ingroup asc_core
 */
template <LittleEndianScalar T>
Status EncodeLittleEndian(T value, std::span<std::byte> destination) {
  if (destination.size() != sizeof(T)) {
    return Status(ErrorCode::kEncoding,
                  "Little-endian destination has the wrong size");
  }
  using Unsigned = internal_core_io::UnsignedScalar<T>;
  Unsigned bits = internal_core_io::ToUnsignedBits(value);
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    destination[index] =
        static_cast<std::byte>(bits & static_cast<Unsigned>(0xffU));
    if constexpr (sizeof(T) > sizeof(std::uint8_t)) {
      bits >>= 8U;
    }
  }
  return Status::Ok();
}

/**
 * @brief Performs the public DecodeLittleEndian operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] source Input source, valid and accessible for the operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
template <LittleEndianScalar T>
Result<T> DecodeLittleEndian(std::span<const std::byte> source) {
  if (source.size() != sizeof(T)) {
    return Status(ErrorCode::kEncoding,
                  "Little-endian source has the wrong size");
  }
  using Unsigned = internal_core_io::UnsignedScalar<T>;
  Unsigned bits = 0;
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    const auto byte =
        static_cast<Unsigned>(std::to_integer<unsigned int>(source[index]));
    bits |= byte << (index * 8U);
  }
  return internal_core_io::FromUnsignedBits<T>(bits);
}

}  // namespace asc

#endif  // ASC_CORE_IO_H_
