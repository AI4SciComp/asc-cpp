#include "asc/core/status.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "asc/core/contracts.h"

namespace asc {

std::string_view ErrorCodeName(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::kOk:
      return "ok";
    case ErrorCode::kInvalidArgument:
      return "invalid-argument";
    case ErrorCode::kShape:
      return "shape";
    case ErrorCode::kIndex:
      return "index";
    case ErrorCode::kOverflow:
      return "overflow";
    case ErrorCode::kInvalidState:
      return "invalid-state";
    case ErrorCode::kAllocation:
      return "allocation";
    case ErrorCode::kMemoryAccess:
      return "memory-access";
    case ErrorCode::kMemoryTransfer:
      return "memory-transfer";
    case ErrorCode::kUnsupported:
      return "unsupported";
    case ErrorCode::kUnavailable:
      return "unavailable";
    case ErrorCode::kProvider:
      return "provider";
    case ErrorCode::kNumerical:
      return "numerical";
    case ErrorCode::kConfiguration:
      return "configuration";
    case ErrorCode::kIo:
      return "io";
    case ErrorCode::kEndOfFile:
      return "end-of-file";
    case ErrorCode::kEncoding:
      return "encoding";
    case ErrorCode::kVersion:
      return "version";
    case ErrorCode::kInternal:
      return "internal";
  }
  return "unknown";
}

Status::Status(ErrorCode code, std::string message, std::string provider,
               std::int64_t native_code) noexcept
    : code_(code),
      message_(std::move(message)),
      provider_(std::move(provider)),
      native_code_(native_code) {
  ASC_CHECK_MESSAGE(
      code_ != ErrorCode::kOk ||
          (message_.empty() && provider_.empty() && native_code_ == 0),
      "An OK Status cannot contain failure diagnostics");
}

Status Status::Ok() noexcept { return {}; }

std::string Status::ToString() const {
  if (ok()) {
    return "ok";
  }
  std::string output(ErrorCodeName(code_));
  if (!provider_.empty()) {
    output.append(" [");
    output.append(provider_);
    if (native_code_ != 0) {
      output.push_back(':');
      output.append(std::to_string(native_code_));
    }
    output.push_back(']');
  } else if (native_code_ != 0) {
    output.append(" [native:");
    output.append(std::to_string(native_code_));
    output.push_back(']');
  }
  if (!message_.empty()) {
    output.append(": ");
    output.append(message_);
  }
  return output;
}

}  // namespace asc
