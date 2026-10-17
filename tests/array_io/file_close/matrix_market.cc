#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string_view>

#include "asc/core/array_io.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#ifdef TEST_DENSE
#include "asc/core/matrix_market.h"
#include "asc/dense/array.h"
#include "asc/dense/layout.h"
#include "asc/dense/matrix_market.h"
#else
#include "asc/sparse/coordinate.h"
#include "asc/sparse/matrix_market.h"
#endif

namespace {

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

using Shape = asc::Extents<2, 2>;
#ifdef TEST_DENSE
using Owner = asc::DenseArray<double, Shape>;
using Report = asc::ArrayIoReport;
constexpr std::string_view kFixture =
    "%%MatrixMarket matrix array real general\n2 2\n2\n-0\n5\n-7\n";
asc::ArrayIoReport& Io(Report& report) { return report; }
#else
using Owner = asc::CoordinateArray<double, Shape>;
using Report = asc::SparseMatrixMarketReport;
constexpr std::string_view kFixture =
    "%%MatrixMarket matrix coordinate real general\n2 2 3\n"
    "1 1 2\n1 2 -0\n2 2 -7\n";
asc::ArrayIoReport& Io(Report& report) { return report.io; }
#endif

asc::Result<Owner> Load(const std::filesystem::path& path, Resource& resource,
                        Report& report) {
  std::array<std::byte, 512> scratch{};
  const asc::ArrayIoLimits limits;
#ifdef TEST_DENSE
  return asc::LoadDenseMatrixMarket<double, Shape>(
      path, resource, asc::LayoutLeft{}, scratch, limits, report);
#else
  return asc::LoadSparseMatrixMarket<Owner>(path, resource, scratch, limits, {},
                                            report);
#endif
}

asc::Status Save(const std::filesystem::path& path, const Owner& owner,
                 Report& report) {
  auto view = owner.view();
  if (!view.ok()) {
    return view.status();
  }
  std::array<std::byte, 512> scratch{};
  const asc::ArrayIoLimits limits;
  const auto overwrite = asc::ArrayFileOverwrite::kTruncate;
#ifdef TEST_DENSE
  return asc::SaveDenseMatrixMarket(path, *view,
                                    asc::MatrixMarketSymmetry::kGeneral,
                                    overwrite, limits, scratch, report);
#else
  return asc::SaveSparseMatrixMarket(path, *view, overwrite, limits, {},
                                     scratch, report);
#endif
}

void Arm(bool close_fault, bool flush_fault = false) {
  fail_close = close_fault;
  fail_flush = flush_fault;
  closes = 0;
  flushes = 0;
}

void CheckLoadFailures(const std::filesystem::path& path) {
  Resource resource;
  Report report;
  Arm(true);
  {
    auto result = Load(path, resource, report);
    Check(!result.ok() && result.status().code() == asc::ErrorCode::kIo,
          "close failure rejects otherwise valid owner");
    Check(!Io(report).committed &&
              Io(report).cleanup_error == asc::ErrorCode::kIo,
          "load does not report publication after close failure");
    Check(Io(report).section == asc::ArrayIoSection::kTrailer,
          "close-only load failure follows payload validation");
    Check(resource.live == 0, "staged owner released before failure returns");
  }
  Check(closes == 1, "failed load closes exactly once");
  resource.reject = true;
  Arm(true);
  {
    auto result = Load(path, resource, report);
    Check(!result.ok() && result.status().code() == asc::ErrorCode::kAllocation,
          "allocation failure remains primary over close failure");
    Check(Io(report).cleanup_error == asc::ErrorCode::kIo &&
              !Io(report).committed,
          "secondary close failure retained after allocation failure");
    Check(resource.live == 0, "allocation rejection leaks no owner");
  }
  Check(closes == 1, "resource-failed load closes exactly once");
  Arm(false);
}

void CheckSaveFailures(const std::filesystem::path& path, const Owner& owner) {
  Report report;
  Arm(true);
  auto status = Save(path, owner, report);
  Check(status.code() == asc::ErrorCode::kIo && status.native_code() == EIO,
        "close-only save failure is returned");
  Check(Io(report).cleanup_error == asc::ErrorCode::kIo &&
            Io(report).section != asc::ArrayIoSection::kComplete,
        "save does not report complete after close failure");
  Check(closes == 1 && flushes == 1, "save flushes then closes exactly once");
  Arm(true, true);
  status = Save(path, owner, report);
  Check(status.code() == asc::ErrorCode::kIo && status.native_code() == ENOSPC,
        "flush error remains primary over secondary close error");
  Check(Io(report).cleanup_error == asc::ErrorCode::kIo,
        "save retains secondary close failure");
  Check(closes == 1 && flushes == 1, "flush-failed save still closes once");
  Arm(false);
  status = Save(path, owner, report);
  Check(status.ok() && Io(report).section == asc::ArrayIoSection::kComplete,
        "control save succeeds after faults disarm");
}

void CheckMalformed(const std::filesystem::path& path) {
  Check(asc::WriteTextFile(path, "invalid magic\n").ok(),
        "write independent malformed fixture");
  Resource resource;
  Report report;
  asc::ErrorCode primary = asc::ErrorCode::kOk;
  {
    auto result = Load(path, resource, report);
    Check(!result.ok(), "malformed control is rejected");
    primary = result.status().code();
  }
  Arm(true);
  {
    auto result = Load(path, resource, report);
    Check(!result.ok() && result.status().code() == primary,
          "malformed input remains primary over close failure");
    Check(Io(report).cleanup_error == asc::ErrorCode::kIo &&
              !Io(report).committed,
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
  Report report;
  {
    auto owner = Load(input, resource, report);
    if (!owner.ok()) {
      std::fprintf(stderr, "Independent fixture failed: %u\n",
                   static_cast<unsigned>(owner.status().code()));
      return 2;
    }
    CheckSaveFailures("saved.mtx", *owner);
    CheckLoadFailures("saved.mtx");
    CheckMalformed("malformed.mtx");
  }
  Check(resource.live == 0, "original owner lifetime balances its resource");
  std::printf("Matrix Market close/flush fault checks=%zu failures=%zu\n",
              checks, failures);
  return failures == 0 ? 0 : 1;
}
