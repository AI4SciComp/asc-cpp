#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <span>

#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#ifdef TEST_DENSE
#include "asc/dense/io.h"
#else
#include "asc/sparse/io.h"
#endif

#include "fixture.h"

namespace {
using asc_file_close_test::kFixture;
using asc_file_close_test::Owner;
#ifdef TEST_DENSE
using asc_file_close_test::Element;
using asc_file_close_test::Layout;
using asc_file_close_test::Shape;
#endif

bool fail_close = false;
bool fail_flush = false;
std::size_t closes = 0;
std::size_t flushes = 0;
std::size_t checks = 0;
std::size_t failures = 0;

void Check(bool passed, const char* label) {
  ++checks;
  if (!passed) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label);
  }
}

class Resource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }
  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    if (reject) {
      return asc::Status(asc::ErrorCode::kAllocation);
    }
    auto result = host_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++live;
    }
    return result;
  }
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      --live;
    }
    host_.Deallocate(pointer, bytes, alignment);
  }
  bool reject = false;
  std::size_t live = 0;

 private:
  asc::HostMemoryResource host_;
};

asc::Result<Owner> Load(const std::filesystem::path& path, Resource& resource,
                        bool binary, asc::ArrayIoReport& report) {
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 512> scratch{};
  const asc::ArrayIoLimits limits;
#ifdef TEST_DENSE
  return binary
             ? asc::LoadDenseArrayBinary<Element, Shape>(
                   path, resource, Layout{}, metadata, scratch, limits, report)
             : asc::LoadDenseArrayText<Element, Shape>(
                   path, resource, Layout{}, metadata, scratch, limits, report);
#else
  return binary ? asc::LoadSparseArrayBinary<Owner>(path, resource, metadata,
                                                    scratch, limits, report)
                : asc::LoadSparseArrayText<Owner>(path, resource, metadata,
                                                  scratch, limits, report);
#endif
}

asc::Status Save(const std::filesystem::path& path, const Owner& owner,
                 bool binary, asc::ArrayIoReport& report) {
  auto view = owner.view();
  if (!view.ok()) {
    return view.status();
  }
  std::array<std::byte, 512> scratch{};
  const asc::ArrayIoLimits limits;
  const auto overwrite = asc::ArrayFileOverwrite::kTruncate;
#ifdef TEST_DENSE
  return binary ? asc::SaveDenseArrayBinary(path, *view, overwrite, limits,
                                            scratch, report)
                : asc::SaveDenseArrayText(path, *view, overwrite, limits,
                                          scratch, report);
#else
  return binary ? asc::SaveSparseArrayBinary(path, *view, overwrite, limits,
                                             scratch, report)
                : asc::SaveSparseArrayText(path, *view, overwrite, limits,
                                           scratch, report);
#endif
}

void Arm(bool close_fault, bool flush_fault = false) {
  fail_close = close_fault;
  fail_flush = flush_fault;
  closes = 0;
  flushes = 0;
}

void CheckLoadFailures(const std::filesystem::path& path, bool binary) {
  Resource resource;
  asc::ArrayIoReport report;
  Arm(true);
  {
    auto result = Load(path, resource, binary, report);
    Check(!result.ok() && result.status().code() == asc::ErrorCode::kIo,
          "close failure rejects otherwise valid owner");
    Check(!report.committed && report.cleanup_error == asc::ErrorCode::kIo,
          "load does not report publication after close failure");
    Check(report.section == asc::ArrayIoSection::kTrailer,
          "close-only load failure follows payload validation");
    Check(resource.live == 0, "staged owner released before failure returns");
  }
  Check(closes == 1, "failed load closes exactly once");
  resource.reject = true;
  Arm(true);
  {
    auto result = Load(path, resource, binary, report);
    Check(!result.ok() && result.status().code() == asc::ErrorCode::kAllocation,
          "allocation failure remains primary over close failure");
    Check(report.cleanup_error == asc::ErrorCode::kIo && !report.committed,
          "secondary close failure retained after allocation failure");
    Check(resource.live == 0, "allocation rejection leaks no owner");
  }
  Check(closes == 1, "resource-failed load closes exactly once");
  Arm(false);
}

void CheckSaveFailures(const std::filesystem::path& path, const Owner& owner,
                       bool binary) {
  asc::ArrayIoReport report;
  Arm(true);
  auto status = Save(path, owner, binary, report);
  Check(status.code() == asc::ErrorCode::kIo && status.native_code() == EIO,
        "close-only save failure is returned");
  Check(report.cleanup_error == asc::ErrorCode::kIo &&
            report.section != asc::ArrayIoSection::kComplete,
        "save does not report complete after close failure");
  Check(closes == 1 && flushes == 1, "save flushes then closes exactly once");
  Arm(true, true);
  status = Save(path, owner, binary, report);
  Check(status.code() == asc::ErrorCode::kIo && status.native_code() == ENOSPC,
        "flush error remains primary over secondary close error");
  Check(report.cleanup_error == asc::ErrorCode::kIo,
        "save retains secondary close failure");
  Check(closes == 1 && flushes == 1, "flush-failed save still closes once");
  Arm(false);
  status = Save(path, owner, binary, report);
  Check(status.ok() && report.section == asc::ArrayIoSection::kComplete,
        "control save succeeds after faults disarm");
}

void CheckMalformed(const std::filesystem::path& path, bool binary) {
  Check(asc::WriteTextFile(path, "invalid magic\n").ok(),
        "write independent malformed fixture");
  Resource resource;
  asc::ArrayIoReport report;
  asc::ErrorCode primary = asc::ErrorCode::kOk;
  {
    auto result = Load(path, resource, binary, report);
    Check(!result.ok(), "malformed control is rejected");
    primary = result.status().code();
  }
  Arm(true);
  {
    auto result = Load(path, resource, binary, report);
    Check(!result.ok() && result.status().code() == primary,
          "malformed input remains primary over close failure");
    Check(report.cleanup_error == asc::ErrorCode::kIo && !report.committed,
          "malformed load retains close failure without publication");
    Check(resource.live == 0, "malformed load releases all resources");
  }
  Check(closes == 1, "malformed load closes exactly once");
  Arm(false);
}

}  // namespace

// GNU --wrap symbol names are required linker spellings, kept out of C++
// identifiers through assembler labels in this Linux-only test adapter.
extern "C" int RealClose(std::FILE* stream) asm("__real_fclose");
extern "C" int WrappedClose(std::FILE* stream) asm("__wrap_fclose");
extern "C" int RealFlush(std::FILE* stream) asm("__real_fflush");
extern "C" int WrappedFlush(std::FILE* stream) asm("__wrap_fflush");

extern "C" int WrappedClose(std::FILE* stream) {
  ++closes;
  const int result = RealClose(stream);
  if (fail_close) {
    errno = EIO;
    return EOF;
  }
  return result;
}

extern "C" int WrappedFlush(std::FILE* stream) {
  ++flushes;
  const int result = RealFlush(stream);
  if (fail_flush) {
    errno = ENOSPC;
    return EOF;
  }
  return result;
}

int main() {
  Resource resource;
  const std::filesystem::path input = "independent.asc";
  Check(asc::WriteTextFile(input, kFixture).ok(), "write independent fixture");
  asc::ArrayIoReport report;
  {
    auto owner = Load(input, resource, false, report);
    if (!owner.ok()) {
      std::fprintf(stderr, "Independent fixture failed: %u\n",
                   static_cast<unsigned>(owner.status().code()));
      return 2;
    }
    for (bool binary : {false, true}) {
      const std::filesystem::path output = binary ? "saved.ascb" : "saved.asc";
      CheckSaveFailures(output, *owner, binary);
      CheckLoadFailures(output, binary);
      CheckMalformed("malformed.asc", binary);
    }
  }
  Check(resource.live == 0, "original owner lifetime balances its resource");
  std::printf("file close/flush fault checks=%zu failures=%zu\n", checks,
              failures);
  return failures == 0 ? 0 : 1;
}
