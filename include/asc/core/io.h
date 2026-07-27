#ifndef ASC_CORE_IO_H_
#define ASC_CORE_IO_H_

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

class ASC_CORE_EXPORT ByteSource {
 public:
  virtual ~ByteSource();

  // Returns zero at end of stream. Implementations are synchronous and do not
  // use zero to report a temporary would-block state. A zero-sized request
  // succeeds without accessing its pointer.
  virtual Result<std::size_t> ReadSome(std::span<std::byte> destination) = 0;
};

class ASC_CORE_EXPORT ByteSink {
 public:
  virtual ~ByteSink();

  // A zero-sized request succeeds without accessing its pointer.
  virtual Result<std::size_t> WriteSome(std::span<const std::byte> source) = 0;
};

ASC_CORE_EXPORT Status ReadExact(ByteSource& source,
                                 std::span<std::byte> destination);
ASC_CORE_EXPORT Status WriteAll(ByteSink& sink,
                                std::span<const std::byte> source);

class ASC_CORE_EXPORT File final : public ByteSource, public ByteSink {
 public:
  static Result<File> OpenRead(const std::filesystem::path& path);
  static Result<File> OpenWrite(const std::filesystem::path& path);

  File(const File&) = delete;
  File& operator=(const File&) = delete;
  File(File&& other) noexcept;
  File& operator=(File&& other) noexcept;
  ~File() override;

  [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }
  [[nodiscard]] bool readable() const noexcept { return readable_; }
  [[nodiscard]] bool writable() const noexcept { return writable_; }

  Result<std::size_t> ReadSome(std::span<std::byte> destination) override;
  Result<std::size_t> WriteSome(std::span<const std::byte> source) override;
  Status Flush();
  Status Close() noexcept;

 private:
  File(void* handle, bool readable, bool writable) noexcept;

  void* handle_ = nullptr;
  bool readable_ = false;
  bool writable_ = false;
};

ASC_CORE_EXPORT Result<std::string> ReadTextFile(
    const std::filesystem::path& path, std::size_t maximum_bytes);
ASC_CORE_EXPORT Status WriteTextFile(const std::filesystem::path& path,
                                     std::string_view text);

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
