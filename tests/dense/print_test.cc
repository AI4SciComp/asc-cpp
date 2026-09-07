#include "asc/dense/print.h"

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
// Required declaration for placement new[] into the mmap-backed test span.
#include <new>  // IWYU pragma: keep
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/contracts.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "test_support.h"

#if defined(__linux__)
#include <sys/mman.h>
#endif

namespace {

using asc_dense_test::TestContext;

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

template <typename T, std::size_t Rank, typename Layout = asc::LayoutLeft>
auto View(T* data, const std::array<asc::extent_t, Rank>& shape,
          Layout layout = {},
          asc::MemorySpace space = asc::MemorySpace::kHost) {
  auto mapping = asc::DenseLayout<Rank>::Create(shape, layout);
  ASC_CHECK(mapping.ok());
  return asc::DenseView<T, Rank>::Create(data, *mapping, space);
}

void TestTinyRanksAndLayouts(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  asc::ArrayPrintReport report;
  double scalar = -0.0;
  auto scalar_view = View(&scalar, std::array<asc::extent_t, 0>{});
  Sink scalar_sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*scalar_view, scalar_sink, options, scratch, report)
                .ok());
  ASC_DENSE_TEST_EQ(test, scalar_sink.text(),
                    "dense scalar=f64 shape=() count=1\n-0\n");
  ASC_DENSE_TEST_EQ(test, report.values_displayed, std::size_t{1});
  ASC_DENSE_TEST_EQ(test, report.output_bytes, scalar_sink.text().size());
  ASC_DENSE_TEST_CHECK(test, !report.truncated);

  std::array<std::int8_t, 3> integers{-128, 0, 127};
  auto vector = View(integers.data(), std::array<asc::extent_t, 1>{3});
  Sink vector_sink;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::PrintArray(*vector, vector_sink, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, vector_sink.text(),
                    "dense scalar=i8 shape=(3) count=3\n[-128, 0, 127]\n");

  std::array<double, 6> left{1, 4, 2, 5, 3, 6};
  std::array<double, 6> right{1, 2, 3, 4, 5, 6};
  auto left_view = View(left.data(), std::array<asc::extent_t, 2>{2, 3});
  auto right_view = View(right.data(), std::array<asc::extent_t, 2>{2, 3},
                         asc::LayoutRight{});
  Sink left_sink;
  Sink right_sink;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::PrintArray(*left_view, left_sink, options, scratch, report).ok());
  ASC_DENSE_TEST_CHECK(
      test,
      asc::PrintArray(*right_view, right_sink, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, left_sink.text(), right_sink.text());
  ASC_DENSE_TEST_EQ(
      test, left_sink.text(),
      "dense scalar=f64 shape=(2,3) count=6\n[[1, 2, 3],\n [4, 5, 6]]\n");

  std::array<int, 8> tensor{1, 3, 2, 4, 5, 7, 6, 8};
  auto tensor_view = View(tensor.data(), std::array<asc::extent_t, 3>{2, 2, 2});
  Sink tensor_sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*tensor_view, tensor_sink, options, scratch, report)
                .ok());
  ASC_DENSE_TEST_EQ(test, tensor_sink.text(),
                    "dense scalar=i32 shape=(2,2,2) count=8\n"
                    "slice axes=(0,1) fixed=(2:0)\n[[1, 2],\n [3, 4]]\n"
                    "slice axes=(0,1) fixed=(2:1)\n[[5, 6],\n [7, 8]]\n");
}

void TestSpecialValuesAndNotation(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  asc::ArrayPrintReport report;
  std::array<std::complex<double>, 3> values{
      std::complex<double>{1.25, -2.5},
      std::complex<double>{-0.0, std::numeric_limits<double>::infinity()},
      std::complex<double>{std::numeric_limits<double>::quiet_NaN(),
                           -std::numeric_limits<double>::infinity()}};
  auto view = View(values.data(), std::array<asc::extent_t, 1>{3});
  Sink general;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, general, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, general.text(),
                    "[(1.25,-2.5), (-0,inf), (nan,-inf)]\n");
  options.float_format = asc::ArrayFloatFormat::kFixed;
  options.precision = 2;
  Sink fixed;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, fixed, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, fixed.text(),
                    "[(1.25,-2.50), (-0.00,inf), (nan,-inf)]\n");
  options.float_format = asc::ArrayFloatFormat::kScientific;
  Sink scientific;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, scientific, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, scientific.text(),
                    "[(1.25e+00,-2.50e+00), (-0.00e+00,inf), (nan,-inf)]\n");
}

template <typename T>
void CheckScalarSpelling(TestContext& test, T value,
                         std::string_view expected) {
  auto view = View(&value, std::array<asc::extent_t, 0>{});
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  asc::ArrayPrintReport report;
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, sink, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, sink.text(), expected);
}

void TestEveryScalarAndTokenFailure(TestContext& test) {
  CheckScalarSpelling(test, std::int8_t{-128}, "-128\n");
  CheckScalarSpelling(test, std::uint8_t{255}, "255\n");
  CheckScalarSpelling(test, std::int16_t{-32768}, "-32768\n");
  CheckScalarSpelling(test, std::uint16_t{65535}, "65535\n");
  CheckScalarSpelling(test, std::numeric_limits<std::int32_t>::min(),
                      "-2147483648\n");
  CheckScalarSpelling(test, std::numeric_limits<std::uint32_t>::max(),
                      "4294967295\n");
  CheckScalarSpelling(test, std::numeric_limits<std::int64_t>::min(),
                      "-9223372036854775808\n");
  CheckScalarSpelling(test, std::numeric_limits<std::uint64_t>::max(),
                      "18446744073709551615\n");
  CheckScalarSpelling(test, 1.25F, "1.25\n");
  CheckScalarSpelling(test, 1.25, "1.25\n");
  CheckScalarSpelling(test, std::complex<float>{1.25F, -2.5F}, "(1.25,-2.5)\n");
  CheckScalarSpelling(test, std::complex<double>{1.25, -2.5}, "(1.25,-2.5)\n");
  double huge = std::numeric_limits<double>::max();
  auto view = View(&huge, std::array<asc::extent_t, 0>{});
  std::array<std::byte, 32> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.float_format = asc::ArrayFloatFormat::kFixed;
  options.precision = 32;
  asc::ArrayPrintReport report;
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test, !asc::PrintArray(*view, sink, options, scratch, report).ok());
  ASC_DENSE_TEST_CHECK(test, sink.text().empty());
  // A representable underlying byte deliberately tests public enum validation.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  options.float_format = static_cast<asc::ArrayFloatFormat>(255);
  ASC_DENSE_TEST_CHECK(
      test, !asc::PrintArray(*view, sink, options, scratch, report).ok());
}

void TestStridesEmptyAndOwners(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  asc::ArrayPrintReport report;
  std::array<double, 10> padding{1, 4, 999, 999, 2, 5, 999, 999, 3, 6};
  const auto before = padding;
  auto view = View(padding.data(), std::array<asc::extent_t, 2>{2, 3},
                   asc::LayoutStride<2>{{1, 4}});
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, sink, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(
      test, sink.text(),
      "dense scalar=f64 shape=(2,3) count=6\n[[1, 2, 3],\n [4, 5, 6]]\n");
  ASC_DENSE_TEST_EQ(test, padding, before);

  auto empty = View(static_cast<double*>(nullptr),
                    std::array<asc::extent_t, 3>{0, 1000000000, 2});
  Sink empty_sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*empty, empty_sink, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, empty_sink.text(),
                    "dense scalar=f64 shape=(0,1000000000,2) count=0\n"
                    "empty shape=(0,1000000000,2)\n");
  ASC_DENSE_TEST_EQ(test, report.values_displayed, std::size_t{0});
  ASC_DENSE_TEST_CHECK(test, !report.truncated);

  asc::HostMemoryResource resource;
  auto extents = asc::Extents<2>::Create();
  auto owner =
      asc::DenseArray<double, asc::Extents<2>>::Create(resource, *extents);
  Sink owner_sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*owner, owner_sink, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, owner_sink.text(),
                    "dense scalar=f64 shape=(2) count=2\n[0, 0]\n");
}

void TestPreviewsAndBudgets(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_elements = 4;
  asc::ArrayPrintReport report;
  std::array<int, 10> values{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  auto view = View(values.data(), std::array<asc::extent_t, 1>{10});
  Sink edges;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, edges, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, edges.text(), "[0, 1, ..., 8, 9]\n... (truncated)\n");
  ASC_DENSE_TEST_EQ(test, report.values_displayed, std::size_t{4});
  options.edge_preview = false;
  Sink prefix;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, prefix, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, prefix.text(),
                    "[0, 1, 2, 3, ...]\n... (truncated)\n");
  options.max_elements = 0;
  Sink zero;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, zero, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, zero.text(), "[...]\n... (truncated)\n");
  ASC_DENSE_TEST_EQ(test, report.values_displayed, std::size_t{0});

  options.max_elements = 64;
  options.edge_preview = true;
  for (std::size_t budget = 0; budget < 100; ++budget) {
    options.max_output_bytes = budget;
    Sink bounded;
    const auto status =
        asc::PrintArray(*view, bounded, options, scratch, report);
    ASC_DENSE_TEST_CHECK(test, bounded.text().size() <= budget);
    ASC_DENSE_TEST_EQ(test, report.output_bytes, bounded.text().size());
    if (status.ok()) {
      ASC_DENSE_TEST_CHECK(test, !report.truncated || bounded.text().ends_with(
                                                          "... (truncated)\n"));
      ASC_DENSE_TEST_CHECK(
          test, report.truncated || report.values_displayed == values.size());
      ASC_DENSE_TEST_CHECK(
          test, bounded.text().find("]\n") != std::string_view::npos);
    }
  }

  auto matrix = View(values.data(), std::array<asc::extent_t, 2>{2, 5});
  options.max_rows = 1;
  options.max_columns = 2;
  options.max_output_bytes = 1024;
  Sink axis;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*matrix, axis, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, axis.text(),
                    "[[0, ..., 8],\n ...]\n... (truncated)\n");
  ASC_DENSE_TEST_EQ(test, report.values_displayed, std::size_t{2});
}

void TestFailuresPlacementAndAllocation(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  asc::ArrayPrintReport report;
  std::array<double, 3> values{1.25, -0.0, 3.5};
  auto view = View(values.data(), std::array<asc::extent_t, 1>{3});
  Sink complete;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, complete, options, scratch, report).ok());
  for (std::size_t failure = 0; failure < complete.text().size(); ++failure) {
    Sink failing;
    failing.max_chunk = 2;
    failing.fail_after = failure;
    const auto status =
        asc::PrintArray(*view, failing, options, scratch, report);
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, report.output_bytes, failure);
    ASC_DENSE_TEST_EQ(test, failing.text(), complete.text().substr(0, failure));
  }
  Sink short_sink;
  short_sink.max_chunk = 1;
  asc_dense_test::AllocationProbe probe;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, short_sink, options, scratch, report).ok());
  ASC_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  ASC_DENSE_TEST_EQ(test, short_sink.text(), complete.text());
  Sink zero;
  zero.zero_progress = true;
  ASC_DENSE_TEST_CHECK(
      test, !asc::PrintArray(*view, zero, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, report.output_bytes, std::size_t{0});
  for (auto space : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
    auto rejected = View(values.data(), std::array<asc::extent_t, 1>{3},
                         asc::LayoutLeft{}, space);
    Sink sink;
    ASC_DENSE_TEST_CHECK(
        test, !asc::PrintArray(*rejected, sink, options, scratch, report).ok());
    ASC_DENSE_TEST_CHECK(test, sink.text().empty());
  }
  auto pinned = View(values.data(), std::array<asc::extent_t, 1>{3},
                     asc::LayoutLeft{}, asc::MemorySpace::kPinnedHost);
  Sink pinned_sink;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::PrintArray(*pinned, pinned_sink, options, scratch, report).ok());
  for (int precision : {0, 33}) {
    options.precision = precision;
    Sink rejected;
    ASC_DENSE_TEST_CHECK(
        test, !asc::PrintArray(*view, rejected, options, scratch, report).ok());
    ASC_DENSE_TEST_CHECK(test, rejected.text().empty());
  }
  options.precision = 6;
  Sink tiny;
  ASC_DENSE_TEST_CHECK(
      test, !asc::PrintArray(*view, tiny, options,
                             std::span<std::byte>(scratch).first(1), report)
                 .ok());
  const auto before = values;
  Sink alias;
  ASC_DENSE_TEST_CHECK(
      test, !asc::PrintArray(*view, alias, options,
                             std::as_writable_bytes(std::span(values)), report)
                 .ok());
  ASC_DENSE_TEST_EQ(test, values, before);
}

void TestHugeLegalPreview(TestContext& test) {
#if defined(__linux__)
  constexpr std::size_t kCount = 1000000000;
  constexpr std::size_t kBytes = kCount * sizeof(double);
  void* allocation = mmap(nullptr, kBytes, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASC_DENSE_TEST_CHECK(test, allocation != MAP_FAILED);
  if (allocation == MAP_FAILED) {
    return;
  }
  // Begin every element's lifetime without initializing/scanning the span.
  auto* values = ::new (allocation) double[kCount];
  values[0] = 1;
  values[1] = 2;
  values[kCount - 2] = 3;
  values[kCount - 1] = 4;
  auto view = View(values, std::array<asc::extent_t, 1>{kCount});
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintOptions options;
  options.show_metadata = false;
  options.max_elements = 4;
  asc::ArrayPrintReport report;
  Sink sink;
  ASC_DENSE_TEST_CHECK(
      test, asc::PrintArray(*view, sink, options, scratch, report).ok());
  ASC_DENSE_TEST_EQ(test, sink.text(), "[1, 2, ..., 3, 4]\n... (truncated)\n");
  ASC_DENSE_TEST_EQ(test, report.values_displayed, std::size_t{4});
  ASC_DENSE_TEST_EQ(test, munmap(allocation, kBytes), 0);
#else
  static_cast<void>(
      test);  // mmap-specific large virtual-span evidence is Linux-only.
#endif
}

void TestMovedLongSinkError(TestContext& test) {
  double value = 4;
  auto view = View(&value, std::array<asc::extent_t, 0>{});
  std::array<std::byte, 1024> scratch{};
  asc::ArrayPrintReport report;
  Sink sink;
  sink.fail_after = 7;
  sink.error = asc::Status(asc::ErrorCode::kIo, std::string(8192, 'x'),
                           std::string(4096, 'p'), 1729);
  asc::Status status;
  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    status = asc::PrintArray(*view, sink, {}, scratch, report);
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
  ASC_DENSE_TEST_EQ(test, status.native_code(), std::int64_t{1729});
  ASC_DENSE_TEST_CHECK(test,
                       status.message().empty() && status.provider().empty());
  ASC_DENSE_TEST_EQ(test, report.output_bytes, std::size_t{7});
}

}  // namespace

int main() {
  TestContext test;
  TestTinyRanksAndLayouts(test);
  TestSpecialValuesAndNotation(test);
  TestEveryScalarAndTokenFailure(test);
  TestStridesEmptyAndOwners(test);
  TestPreviewsAndBudgets(test);
  TestFailuresPlacementAndAllocation(test);
  TestHugeLegalPreview(test);
  TestMovedLongSinkError(test);
  return test.Finish();
}
