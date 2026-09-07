#include "asc/core/array_format.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc::internal_array_format {
namespace {

// WriteAll owns retry semantics. This adapter records partial progress and
// rejects zero/invalid progress before WriteAll constructs a diagnostic.
class CountingSink final : public ByteSink {
 public:
  CountingSink(ByteSink& sink, ArrayPrintReport& report)
      : sink_(sink), report_(report) {}
  Result<std::size_t> WriteSome(std::span<const std::byte> bytes) override {
    auto written = sink_.WriteSome(bytes);
    if (!written.ok()) {
      return written.status();
    }
    if (*written > bytes.size() || (!bytes.empty() && *written == 0)) {
      return Status(ErrorCode::kIo);
    }
    report_.output_bytes += *written;
    return *written;
  }

 private:
  ByteSink& sink_;
  ArrayPrintReport& report_;
};

}  // namespace

Status ValidateOptions(const ArrayPrintOptions& options) {
  if (options.precision < 1 || options.precision > 32 ||
      (options.float_format != ArrayFloatFormat::kGeneral &&
       options.float_format != ArrayFloatFormat::kFixed &&
       options.float_format != ArrayFloatFormat::kScientific)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

bool Overlaps(const void* first, std::size_t first_bytes, const void* second,
              std::size_t second_bytes) {
  if (first_bytes == 0 || second_bytes == 0) {
    return false;
  }
  const auto left = reinterpret_cast<std::uintptr_t>(first);
  const auto right = reinterpret_cast<std::uintptr_t>(second);
  const auto maximum = std::numeric_limits<std::uintptr_t>::max();
  if (first_bytes > maximum - left || second_bytes > maximum - right) {
    return true;
  }
  return left < right + second_bytes && right < left + first_bytes;
}

bool Output::Fits(std::size_t bytes, std::size_t reserve) const {
  if (report_.output_bytes > limit_) {
    return false;
  }
  const std::size_t available = limit_ - report_.output_bytes;
  return reserve <= available && bytes <= available - reserve;
}

bool Output::Write(std::string_view text) {
  if (!status_.ok()) {
    return false;
  }
  if (!Fits(text.size())) {
    Fail(ErrorCode::kAllocation);
    return false;
  }
  if (sink_ == nullptr) {
    report_.output_bytes += text.size();
    return true;
  }
  CountingSink counted(*sink_, report_);
  status_ =
      WriteAll(counted, std::as_bytes(std::span(text.data(), text.size())));
  return status_.ok();
}

}  // namespace asc::internal_array_format
