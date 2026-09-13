#include "observation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {

// This TU is compiled without SanitizerCoverage to avoid observing the
// observer. No production implementation, memory accessor or status is
// replaced.
std::span<asc_display_observation::Region> g_regions;

void Observe(const void* address, std::size_t size) {
  const auto begin = reinterpret_cast<std::uintptr_t>(address);
  for (auto& region : g_regions) {
    const auto region_begin =
        reinterpret_cast<std::uintptr_t>(region.backing.data());
    const auto overlap_begin = std::max(begin, region_begin);
    const auto overlap_end =
        std::min(begin + size, region_begin + region.backing.size());
    for (auto byte = overlap_begin; byte < overlap_end; ++byte) {
      ++region.reads[byte - region_begin];
    }
  }
}

}  // namespace

namespace asc_display_observation {

void Start(std::span<Region> regions) {
  for (auto& region : regions) {
    if (region.backing.size() != region.reads.size()) {
      std::abort();
    }
    std::fill(region.reads.begin(), region.reads.end(), 0);
  }
  g_regions = regions;
}

void Stop() { g_regions = {}; }

}  // namespace asc_display_observation

// The documented callbacks have C linkage and receive the load address.
// void* keeps the 16-byte hook free from nonstandard integer declarations.
// NOLINTNEXTLINE(bugprone-reserved-identifier): required Clang callback ABI.
extern "C" void __sanitizer_cov_trace_pc() {}
// NOLINTNEXTLINE(bugprone-reserved-identifier): required Clang callback ABI.
extern "C" void __sanitizer_cov_load1(void* address) { Observe(address, 1); }
// NOLINTNEXTLINE(bugprone-reserved-identifier): required Clang callback ABI.
extern "C" void __sanitizer_cov_load2(void* address) { Observe(address, 2); }
// NOLINTNEXTLINE(bugprone-reserved-identifier): required Clang callback ABI.
extern "C" void __sanitizer_cov_load4(void* address) { Observe(address, 4); }
// NOLINTNEXTLINE(bugprone-reserved-identifier): required Clang callback ABI.
extern "C" void __sanitizer_cov_load8(void* address) { Observe(address, 8); }
// NOLINTNEXTLINE(bugprone-reserved-identifier): required Clang callback ABI.
extern "C" void __sanitizer_cov_load16(void* address) { Observe(address, 16); }
