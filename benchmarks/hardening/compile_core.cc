#include <cstddef>
#include <cstdint>

#include "asc/core.h"

std::uint64_t AscM8CoreObservation(std::size_t bytes) {
  asc::MutableMemoryView view(nullptr, 0, asc::MemorySpace::kHost);
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  return static_cast<std::uint64_t>(bytes + view.size()) +
         static_cast<std::uint64_t>(context.device().ordinal);
}
