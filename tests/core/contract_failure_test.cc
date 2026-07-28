#include <string_view>
#include <utility>

#include "asc/core/contracts.h"
#include "asc/core/io.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    return 2;
  }
  const std::string_view mode(argv[1]);
  if (mode == "check") {
    ASC_CHECK_MESSAGE(false, "intentional ASC_CHECK test failure");
  }
  if (mode == "result") {
    asc::Result<int> failure(
        asc::Status(asc::ErrorCode::kInvalidState, "no value"));
    return failure.value();
  }
  if (mode == "dcheck") {
    ASC_DCHECK_MESSAGE(false, "intentional ASC_DCHECK test failure");
    return 0;
  }
  if (mode == "dcheck-enabled") {
#if defined(NDEBUG)
    return 0;
#else
    return 3;
#endif
  }
  if (mode == "file-move-assignment") {
    auto destination =
        asc::File::OpenWrite("open move-assignment destination.tmp");
    auto source = asc::File::OpenWrite("open move-assignment source.tmp");
    if (!destination.ok() || !source.ok()) {
      return 4;
    }
    *destination = std::move(*source);
    return 0;
  }
  return 2;
}
