#ifndef ASC_TESTS_ARRAY_IO_FILE_CLEANUP_TEST_SUPPORT_H_
#define ASC_TESTS_ARRAY_IO_FILE_CLEANUP_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <utility>

#include "../array_display_test_support.h"
#include "asc/core/array_io.h"
#include "asc/core/contracts.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc_file_cleanup_test {

using asc_array_display_test::TestContext;

// Every fault belongs to one synchronous call. Actual I/O still passes through
// the linked Core File, including in DLL/shared builds; no symbol interposition
// or process-global fault registry is involved.
struct Fault {
  std::size_t fail_read_after = std::numeric_limits<std::size_t>::max();
  std::size_t fail_write_after = std::numeric_limits<std::size_t>::max();
  bool fail_flush = false;
  bool fail_close = false;
  std::size_t opens = 0;
  std::size_t closes = 0;
  std::size_t flushes = 0;
  std::size_t implicit_closes = 0;
  std::size_t live = 0;
  std::size_t read_bytes = 0;
  std::size_t written_bytes = 0;
};

class Handle final : public asc::ByteSource, public asc::ByteSink {
 public:
  Handle(asc::File file, Fault& fault)
      : file_(std::move(file)), fault_(&fault) {
    ++fault_->opens;
    ++fault_->live;
  }
  ~Handle() override {
    if (fault_ != nullptr && !closed_) {
      ++fault_->implicit_closes;
      --fault_->live;
    }
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle(Handle&& other) noexcept
      : file_(std::move(other.file_)),
        fault_(std::exchange(other.fault_, nullptr)),
        closed_(other.closed_) {}
  Handle& operator=(Handle&&) = delete;

  asc::Result<std::size_t> ReadSome(std::span<std::byte> bytes) override {
    if (bytes.empty()) {
      return std::size_t{0};
    }
    if (fault_->read_bytes == fault_->fail_read_after) {
      return asc::Status(asc::ErrorCode::kIo, {}, {}, 101);
    }
    auto result = file_.ReadSome(bytes.first(
        std::min(bytes.size(), fault_->fail_read_after - fault_->read_bytes)));
    if (result.ok()) {
      fault_->read_bytes += *result;
    }
    return result;
  }
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> bytes) override {
    if (bytes.empty()) {
      return std::size_t{0};
    }
    if (fault_->written_bytes == fault_->fail_write_after) {
      return asc::Status(asc::ErrorCode::kIo, {}, {}, 102);
    }
    auto result = file_.WriteSome(bytes.first(std::min(
        bytes.size(), fault_->fail_write_after - fault_->written_bytes)));
    if (result.ok()) {
      fault_->written_bytes += *result;
    }
    return result;
  }
  asc::Status Flush() {
    ++fault_->flushes;
    auto status = file_.Flush();
    if (!status.ok()) {
      return status;
    }
    return fault_->fail_flush ? asc::Status(asc::ErrorCode::kIo, {}, {}, 103)
                              : asc::Status{};
  }
  asc::Status Close() {
    ASC_CHECK(!closed_);
    closed_ = true;
    ++fault_->closes;
    --fault_->live;
    auto status = file_.Close();
    if (!status.ok()) {
      return status;
    }
    return fault_->fail_close ? asc::Status(asc::ErrorCode::kIo, {}, {}, 104)
                              : asc::Status{};
  }

 private:
  asc::File file_;
  Fault* fault_;
  bool closed_ = false;
};

inline auto Opener(Fault& fault, bool write) {
  return [&fault,
          write](const std::filesystem::path& path) -> asc::Result<Handle> {
    auto file = write ? asc::File::OpenWrite(path) : asc::File::OpenRead(path);
    if (!file.ok()) {
      return file.status();
    }
    return Handle(std::move(*file), fault);
  };
}

class Resource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }
  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++requests;
    if (reject) {
      return asc::Status(asc::ErrorCode::kAllocation, {}, {}, 105);
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
  std::size_t requests = 0;
  std::size_t live = 0;

 private:
  asc::HostMemoryResource host_;
};

inline void CheckClosed(TestContext& test, const Fault& fault) {
  test.Check(fault.opens == 1 && fault.closes == 1 && fault.live == 0 &&
                 fault.implicit_closes == 0,
             "one acquired real File is explicitly closed once, without leak");
}

// Save/Load adapters select existing module/format APIs, not cleanup logic.
// A null fault selects the public path API as the uninjected integration
// control.
template <typename Adapter, typename Owner>
void Saves(TestContext& test, const std::filesystem::path& path,
           const Adapter& adapter, const Owner& owner) {
  for (int mode = 0; mode < 6; ++mode) {
    Fault fault;
    fault.fail_close = mode != 0 && mode != 5;
    fault.fail_flush = mode == 2 || mode == 3 || mode == 5;
    if (mode == 3) {
      fault.fail_write_after = 7;
    }
    asc::ArrayIoLimits limits;
    if (mode == 4) {
      limits.max_output_bytes = 0;
    }
    typename Adapter::Report report;
    auto status = adapter.Save(path, owner, &fault, limits, report);
    const auto& io = adapter.Io(report);
    const auto code = mode == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kIo;
    if (mode != 4) {
      test.Check(status.code() == code, "save primary category");
    }
    constexpr std::array<std::int64_t, 6> kNative{0, 104, 103, 102, 0, 103};
    const auto expected_native = kNative[static_cast<std::size_t>(mode)];
    if (mode != 4) {
      test.Check(status.native_code() == expected_native,
                 "write then flush then close primary error precedence");
    } else {
      test.Check(!status.ok() && status.code() != asc::ErrorCode::kIo,
                 "post-open encoding budget error precedes failed close");
    }
    test.Check(io.cleanup_error == (fault.fail_close ? asc::ErrorCode::kIo
                                                     : asc::ErrorCode::kOk),
               "secondary close diagnostic retained");
    test.Check(!io.committed, "save never claims destination publication");
    test.Check((io.section == asc::ArrayIoSection::kComplete) == (mode == 0),
               "complete only after successful write, flush and close");
    test.Check(fault.flushes == ((mode < 3 || mode == 5) ? 1U : 0U),
               "flush attempted only after successful codec operation");
    test.Check(io.output_bytes == fault.written_bytes,
               "save retains actual accepted byte count");
    if (mode == 3) {
      test.Check(io.output_bytes == 7, "short write prefix kept");
    }
    CheckClosed(test, fault);
  }
}

template <typename Adapter>
void Loads(TestContext& test, const std::filesystem::path& path,
           const Adapter& adapter, bool allocates) {
  for (int mode = 0; mode < 4; ++mode) {
    Fault fault;
    fault.fail_close = mode != 0;
    if (mode == 2) {
      fault.fail_read_after = 7;
    }
    Resource resource;
    resource.reject = mode == 3;
    typename Adapter::Report report;
    {
      auto result = adapter.Load(path, resource, &fault, report);
      const auto& io = adapter.Io(report);
      test.Check(result.ok() == (mode == 0), "close gates owner publication");
      test.Check(io.committed == (mode == 0), "report matches publication");
      test.Check(io.cleanup_error ==
                     (mode == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kIo),
                 "load retains secondary close diagnostic");
      if (mode != 0) {
        const auto expected = mode == 3 && allocates
                                  ? asc::ErrorCode::kAllocation
                                  : asc::ErrorCode::kIo;
        test.Check(result.status().code() == expected,
                   "read/allocation primary is preserved over close");
        if (mode == 1 || mode == 2 || (mode == 3 && !allocates)) {
          test.Check(result.status().native_code() == (mode == 2 ? 101 : 104),
                     "read/close native error is retained");
        }
        test.Check(resource.live == 0, "failed owner disposed before return");
      }
      test.Check(io.input_bytes == fault.read_bytes,
                 "destination failure does not rewind consumed input");
      if (mode == 2) {
        test.Check(io.input_bytes == 7, "short read prefix kept");
      }
    }
    test.Check(resource.live == 0, "all owner resources released");
    test.Check(fault.flushes == 0, "load never flushes");
    CheckClosed(test, fault);
  }
}

template <typename Adapter>
void Malformed(TestContext& test, const std::filesystem::path& path,
               const Adapter& adapter) {
  test.Check(asc::WriteTextFile(path, "invalid magic\n").ok(),
             "independent malformed fixture");
  asc::ErrorCode original = asc::ErrorCode::kOk;
  auto section = asc::ArrayIoSection::kHeader;
  for (bool close : {false, true}) {
    Fault fault;
    fault.fail_close = close;
    Resource resource;
    typename Adapter::Report report;
    auto result = adapter.Load(path, resource, &fault, report);
    test.Check(!result.ok(), "malformed data is rejected");
    if (!close) {
      original = result.status().code();
      section = adapter.Io(report).section;
    }
    test.Check(adapter.Io(report).section ==
                   (close ? adapter.MalformedCloseSection(section) : section),
               "cleanup preserves the documented early-parse phase policy");
    test.Check(
        result.status().code() == original && original != asc::ErrorCode::kIo,
        "parse error remains primary over close");
    test.Check(adapter.Io(report).cleanup_error ==
                   (close ? asc::ErrorCode::kIo : asc::ErrorCode::kOk),
               "parse failure retains secondary cleanup category");
    test.Check(!adapter.Io(report).committed && resource.live == 0,
               "parse failure cannot publish or leak owner");
    CheckClosed(test, fault);
  }
}

template <typename Adapter, typename Owner>
void Profile(TestContext& test, const std::filesystem::path& path,
             const Adapter& adapter, const Owner& owner) {
  Saves(test, path, adapter, owner);
  typename Adapter::Report report;
  test.Check(adapter.Save(path, owner, nullptr, {}, report).ok(),
             "actual public path save succeeds after independent faults");
  bool allocates = false;
  {
    Resource resource;
    auto result = adapter.Load(path, resource, nullptr, report);
    allocates = resource.requests != 0;
    test.Check(result.ok() && adapter.Io(report).committed,
               "actual public path load uses linked Core File successfully");
  }
  Loads(test, path, adapter, allocates);
  Malformed(test, path, adapter);
}

}  // namespace asc_file_cleanup_test

#endif  // ASC_TESTS_ARRAY_IO_FILE_CLEANUP_TEST_SUPPORT_H_
