#ifndef ASC_CORE_STATUS_H_
#define ASC_CORE_STATUS_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "asc/core/export.h"

namespace asc {

// Stable machine-readable error categories. Diagnostic message text is not a
// stable interface.
// The fixed 32-bit representation is part of the public error-code ABI.
// NOLINTNEXTLINE(performance-enum-size)
enum class ErrorCode : std::uint32_t {
  kOk = 0,
  kInvalidArgument = 1,
  kShape = 2,
  kIndex = 3,
  kOverflow = 4,
  kInvalidState = 5,
  kAllocation = 6,
  kMemoryAccess = 7,
  kMemoryTransfer = 8,
  kUnsupported = 9,
  kUnavailable = 10,
  kProvider = 11,
  kNumerical = 12,
  kConfiguration = 13,
  kIo = 14,
  kEndOfFile = 15,
  kEncoding = 16,
  kVersion = 17,
  kInternal = 18,
};

ASC_CORE_EXPORT std::string_view ErrorCodeName(ErrorCode code) noexcept;

class [[nodiscard]] Status {
 public:
  Status() noexcept = default;
  explicit ASC_CORE_EXPORT Status(ErrorCode code, std::string message = {},
                                  std::string provider = {},
                                  std::int64_t native_code = 0) noexcept;

  static ASC_CORE_EXPORT Status Ok() noexcept;

  [[nodiscard]] bool ok() const noexcept { return code_ == ErrorCode::kOk; }
  [[nodiscard]] ErrorCode code() const noexcept { return code_; }
  [[nodiscard]] const std::string& message() const noexcept { return message_; }
  [[nodiscard]] const std::string& provider() const noexcept {
    return provider_;
  }
  [[nodiscard]] std::int64_t native_code() const noexcept {
    return native_code_;
  }

  [[nodiscard]] ASC_CORE_EXPORT std::string ToString() const;

 private:
  ErrorCode code_ = ErrorCode::kOk;
  std::string message_;
  std::string provider_;
  std::int64_t native_code_ = 0;
};

}  // namespace asc

#endif  // ASC_CORE_STATUS_H_
