#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

#include "asc/core/result.h"
#include "asc/core/status.h"

namespace {

bool fail_allocation = false;

}  // namespace

void* operator new(std::size_t size) {
  if (fail_allocation) {
    std::fputs("allocation attempted before FatalContract\n", stderr);
    std::abort();
  }
  if (void* memory = std::malloc(size); memory != nullptr) {
    return memory;
  }
  throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }

void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
  asc::Result<int> failure(
      asc::Status(asc::ErrorCode::kInvalidState, "no value"));
  fail_allocation = true;
  return failure.value();
}
