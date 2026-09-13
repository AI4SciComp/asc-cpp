#include "asc/sparse/print.h"

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "test_resources.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> bytes) override {
    if (zero_progress) {
      return std::size_t{0};
    }
    if (size_ == fail_after) {
      if (!error.ok()) {
        return std::move(error);
      }
      return asc::Status(asc::ErrorCode::kIo);
    }
    const std::size_t count = std::min(
        {bytes.size(), max_chunk, data_.size() - size_, fail_after - size_});
    for (std::size_t i = 0; i < count; ++i) {
      data_[size_++] = static_cast<char>(bytes[i]);
    }
    return count;
  }
  [[nodiscard]] std::string_view text() const { return {data_.data(), size_}; }
  std::size_t max_chunk = std::numeric_limits<std::size_t>::max();
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  bool zero_progress = false;
  asc::Status error;

 private:
  std::array<char, 32768> data_{};
  std::size_t size_ = 0;
};

void TestCoordinateRanksStoredZerosAndComplex(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  asc::ArrayPrintReport report;
  std::int8_t zero = 0;
  auto scalar = asc::CoordinateView<std::int8_t, 0>::Create(
      nullptr, &zero, std::array<asc::extent_t, 0>{}, 1,
      asc::MemorySpace::kHost);
  Sink scalar_sink;
  ASC_SPARSE_TEST_CHECK(
      test,
      asc::PrintArray(*scalar, scalar_sink, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, scalar_sink.text(),
                     "coo scalar=i8 shape=() stored=1\n() = 0\n");
  const std::array<asc::index_t, 4> coordinates{0, 2, 1, 0};
  std::array<std::complex<double>, 2> values{std::complex<double>{-0.0, 0.0},
                                             std::complex<double>{2, -3}};
  auto view = asc::CoordinateView<std::complex<double>, 2>::Create(
      coordinates.data(), values.data(), std::array<asc::extent_t, 2>{2, 3}, 2,
      asc::MemorySpace::kHost);
  Sink sink;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*view, sink, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(
      test, sink.text(),
      "coo scalar=c128 shape=(2,3) stored=2\n(0,2) = (-0,0)\n(1,0) = (2,-3)\n");
  ASC_SPARSE_TEST_EQ(test, report.values_displayed, std::size_t{2});
  ASC_SPARSE_TEST_CHECK(test, !report.truncated);

  const std::array<asc::index_t, 3> coordinate3{1, 2, 3};
  double value3 = 9;
  auto rank3 = asc::CoordinateView<double, 3>::Create(
      coordinate3.data(), &value3, std::array<asc::extent_t, 3>{2, 3, 4}, 1,
      asc::MemorySpace::kHost);
  Sink sink3;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*rank3, sink3, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, sink3.text(),
                     "coo scalar=f64 shape=(2,3,4) stored=1\n(1,2,3) = 9\n");
}

template <asc::SparseCompressedFormat Format>
void TestCompressed(TestContext& test, std::string_view expected) {
  const std::array<asc::nnz_t, 4> offsets{0, 1, 1, 3};
  const std::array<asc::index_t, 3> indices{2, 0, 2};
  std::array<int, 3> values{0, -7, 9};
  auto view = asc::CompressedSparseView<int, Format>::Create(
      offsets.data(), indices.data(), values.data(),
      std::array<asc::extent_t, 2>{3, 3}, 3, asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  asc::ArrayPrintReport report;
  Sink sink;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*view, sink, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, sink.text(), expected);
  ASC_SPARSE_TEST_EQ(test, report.values_displayed, std::size_t{3});
  asc_sparse_test::TrackingMemoryResource resource;
  auto owner = asc::CompressedSparseArray<int, Format>::Create(
      resource, std::array<asc::extent_t, 2>{3, 3}, offsets, indices, values);
  Sink owner_sink;
  const auto allocations = resource.allocation_attempts();
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*owner, owner_sink, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, owner_sink.text(), expected);
  ASC_SPARSE_TEST_EQ(test, resource.allocation_attempts(), allocations);
}

void TestPreviewFailuresAndBudgets(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  const std::array<asc::index_t, 5> coordinates{0, 1, 2, 3, 999999999};
  std::array<int, 5> values{0, 1, 2, 3, 4};
  auto view = asc::CoordinateView<int, 1>::Create(
      coordinates.data(), values.data(),
      std::array<asc::extent_t, 1>{1000000000}, 5, asc::MemorySpace::kHost);
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_elements = 2;
  asc::ArrayPrintReport report;
  Sink preview;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*view, preview, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, preview.text(),
                     "(0) = 0\n...\n(999999999) = 4\n... (truncated)\n");
  ASC_SPARSE_TEST_EQ(test, report.values_displayed, std::size_t{2});
  options.max_elements = 0;
  Sink none;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*view, none, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, none.text(), "... (truncated)\n");
}

void TestFailureProgressAndLimits(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  const std::array<asc::index_t, 5> coordinates{0, 1, 2, 3, 999999999};
  std::array<int, 5> values{0, 1, 2, 3, 4};
  auto view = asc::CoordinateView<int, 1>::Create(
      coordinates.data(), values.data(),
      std::array<asc::extent_t, 1>{1000000000}, 5, asc::MemorySpace::kHost);
  asc::ArrayPrintOptions options;
  asc::ArrayPrintReport report;
  Sink complete;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*view, complete, options, scratch, report).ok());
  for (std::size_t boundary = 0; boundary < complete.text().size();
       ++boundary) {
    Sink failing;
    failing.fail_after = boundary;
    failing.max_chunk = 3;
    ASC_SPARSE_TEST_CHECK(
        test, !asc::PrintArray(*view, failing, options, scratch, report).ok());
    ASC_SPARSE_TEST_EQ(test, report.output_bytes, boundary);
    ASC_SPARSE_TEST_EQ(test, failing.text(),
                       complete.text().substr(0, boundary));
  }
  for (std::size_t budget = 0; budget < complete.text().size() + 1; ++budget) {
    options.max_output_bytes = budget;
    Sink bounded;
    auto status = asc::PrintArray(*view, bounded, options, scratch, report);
    ASC_SPARSE_TEST_CHECK(test, bounded.text().size() <= budget);
    ASC_SPARSE_TEST_EQ(test, report.output_bytes, bounded.text().size());
    if (status.ok()) {
      ASC_SPARSE_TEST_CHECK(
          test, report.truncated || report.values_displayed == values.size());
      ASC_SPARSE_TEST_CHECK(
          test,
          !report.truncated || bounded.text().ends_with("... (truncated)\n"));
    }
  }
  options.max_output_bytes = 16384;
  Sink one_byte;
  one_byte.max_chunk = 1;
  asc_sparse_test::AllocationProbe probe;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*view, one_byte, options, scratch, report).ok());
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  ASC_SPARSE_TEST_EQ(test, one_byte.text(), complete.text());
}

void TestValidationAndScratch(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  const std::array<asc::index_t, 5> coordinates{0, 1, 2, 3, 999999999};
  std::array<int, 5> values{0, 1, 2, 3, 4};
  auto view = asc::CoordinateView<int, 1>::Create(
      coordinates.data(), values.data(),
      std::array<asc::extent_t, 1>{1000000000}, 5, asc::MemorySpace::kHost);
  asc::ArrayPrintOptions options;
  asc::ArrayPrintReport report;
  Sink zero;
  zero.zero_progress = true;
  ASC_SPARSE_TEST_CHECK(
      test, !asc::PrintArray(*view, zero, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, report.output_bytes, std::size_t{0});
  Sink tiny;
  ASC_SPARSE_TEST_CHECK(
      test, !asc::PrintArray(*view, tiny, options,
                             std::span<std::byte>(scratch).first(1), report)
                 .ok());
  const auto before = values;
  Sink alias;
  ASC_SPARSE_TEST_CHECK(
      test, !asc::PrintArray(*view, alias, options,
                             std::as_writable_bytes(std::span(values)), report)
                 .ok());
  ASC_SPARSE_TEST_EQ(test, values, before);
  options.precision = 33;
  Sink invalid;
  ASC_SPARSE_TEST_CHECK(
      test, !asc::PrintArray(*view, invalid, options, scratch, report).ok());
  ASC_SPARSE_TEST_CHECK(test, invalid.text().empty());
  options.precision = 6;
  for (auto placement : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
                         asc::MemorySpace::kPinnedHost}) {
    auto rejected = asc::CoordinateView<int, 1>::Create(
        coordinates.data(), values.data(),
        std::array<asc::extent_t, 1>{1000000000}, 5, placement);
    Sink sink;
    ASC_SPARSE_TEST_CHECK(
        test, !asc::PrintArray(*rejected, sink, options, scratch, report).ok());
    ASC_SPARSE_TEST_CHECK(test, sink.text().empty());
  }
}

void TestEmptyAndCoordinateOwner(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  asc::ArrayPrintReport report;
  const asc::nnz_t offset = 0;
  auto empty = asc::CsrView<double>::Create(&offset, nullptr, nullptr,
                                            std::array<asc::extent_t, 2>{0, 3},
                                            0, asc::MemorySpace::kHost);
  Sink sink;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*empty, sink, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, sink.text(),
                     "csr scalar=f64 shape=(0,3) stored=0\n[]\n");
  ASC_SPARSE_TEST_CHECK(test, !report.truncated);
  asc_sparse_test::TrackingMemoryResource resource;
  using Shape = asc::Extents<2>;
  auto shape = Shape::Create();
  auto builder =
      asc::CoordinateBuilder<int, Shape>::Create(resource, *shape, 1);
  ASC_SPARSE_TEST_CHECK(test,
                        builder->Add(std::array<asc::index_t, 1>{1}, 7).ok());
  auto owner = std::move(*builder).Finalize(asc::ExecutionContext::Serial(),
                                            asc::DuplicatePolicy::kReject,
                                            asc::ExplicitZeroPolicy::kKeep);
  Sink owner_sink;
  ASC_SPARSE_TEST_CHECK(
      test, asc::PrintArray(*owner, owner_sink, options, scratch, report).ok());
  ASC_SPARSE_TEST_EQ(test, owner_sink.text(),
                     "coo scalar=i32 shape=(2) stored=1\n(1) = 7\n");
}

void TestMovedLongSinkError(TestContext& test) {
  double value = 4;
  auto view = asc::CoordinateView<double, 0>::Create(
      nullptr, &value, std::array<asc::extent_t, 0>{}, 1,
      asc::MemorySpace::kHost);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintReport report;
  Sink sink;
  sink.fail_after = 7;
  sink.error = asc::Status(asc::ErrorCode::kIo, std::string(8192, 'x'),
                           std::string(4096, 'p'), 1729);
  asc::Status status;
  std::size_t allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    status = asc::PrintArray(*view, sink, {}, scratch, report);
    allocations = probe.count();
  }
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_SPARSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
  ASC_SPARSE_TEST_EQ(test, status.native_code(), std::int64_t{1729});
  ASC_SPARSE_TEST_CHECK(test,
                        status.message().empty() && status.provider().empty());
  ASC_SPARSE_TEST_EQ(test, report.output_bytes, std::size_t{7});
}

}  // namespace

int main() {
  TestContext test;
  TestCoordinateRanksStoredZerosAndComplex(test);
  TestCompressed<asc::SparseCompressedFormat::kCsr>(
      test,
      "csr scalar=i32 shape=(3,3) stored=3\n(0,2) = 0\n(2,0) = -7\n(2,2) = "
      "9\n");
  TestCompressed<asc::SparseCompressedFormat::kCsc>(
      test,
      "csc scalar=i32 shape=(3,3) stored=3\n(2,0) = 0\n(0,2) = -7\n(2,2) = "
      "9\n");
  TestPreviewFailuresAndBudgets(test);
  TestFailureProgressAndLimits(test);
  TestValidationAndScratch(test);
  TestEmptyAndCoordinateOwner(test);
  TestMovedLongSinkError(test);
  return test.Finish();
}
