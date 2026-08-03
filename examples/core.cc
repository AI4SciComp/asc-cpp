#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "asc/core/configuration.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

int main() {
  const asc::ConfigurationValue configured(std::int64_t{42});
  const asc::Result<std::int64_t> value = configured.AsSignedInteger();
  if (!value.ok() || *value != 42) {
    return 1;
  }

  std::array<std::byte, sizeof(std::uint32_t)> encoded{};
  const asc::Status encoded_status =
      asc::EncodeLittleEndian<std::uint32_t>(0x01020304U, encoded);
  if (!encoded_status.ok()) {
    return 2;
  }
  const auto decoded = asc::DecodeLittleEndian<std::uint32_t>(
      std::span<const std::byte>(encoded));
  return decoded.ok() && *decoded == 0x01020304U ? 0 : 3;
}
