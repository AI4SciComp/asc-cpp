#include "asc/core/contracts.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace asc {

[[noreturn]] void FatalContract(const char* expression, const char* file,
                                int line, std::string_view message) noexcept {
  std::fputs("ASC fatal contract: ", stderr);
  std::fputs(expression == nullptr ? "<unknown expression>" : expression,
             stderr);
  std::fputs(" at ", stderr);
  std::fputs(file == nullptr ? "<unknown file>" : file, stderr);
  std::fprintf(stderr, ":%d", line);
  if (!message.empty()) {
    std::fputs(": ", stderr);
    std::fwrite(message.data(), 1, message.size(), stderr);
  }
  std::fputc('\n', stderr);
  std::fflush(stderr);
  std::abort();
}

}  // namespace asc
