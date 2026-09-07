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
#include "asc/core/array_io.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/io.h"
#include "asc/dense/layout.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

class FailureResource final : public asc::MemoryResource {
 public:
  FailureResource()
      : failure_(asc::ErrorCode::kAllocation, std::string(8192, 'm'),
                 std::string(4096, 'p'), -1729) {}
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }
  asc::Result<void*> Allocate(std::size_t /*bytes*/,
                              std::size_t /*alignment*/) override {
    ++requests_;
    return std::move(failure_);
  }
  void Deallocate(void* /*pointer*/, std::size_t /*bytes*/,
                  std::size_t /*alignment*/) noexcept override {
    ++deallocations_;
  }
  [[nodiscard]] int requests() const { return requests_; }
  [[nodiscard]] int deallocations() const { return deallocations_; }

 private:
  asc::Status failure_;
  int requests_ = 0;
  int deallocations_ = 0;
};

void CheckStatus(TestContext& test, const asc::Status& status,
                 const FailureResource& resource, std::size_t allocations) {
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kAllocation);
  ASC_DENSE_TEST_EQ(test, status.native_code(), -1729);
  ASC_DENSE_TEST_EQ(test, status.message(), std::string(8192, 'm'));
  ASC_DENSE_TEST_EQ(test, status.provider(), std::string(4096, 'p'));
  ASC_DENSE_TEST_EQ(test, resource.requests(), 1);
  ASC_DENSE_TEST_EQ(test, resource.deallocations(), 0);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

void CheckBuffer(TestContext& test) {
  for (auto bytes : {std::size_t{0}, std::size_t{128}}) {
    FailureResource resource;
    const asc_dense_test::AllocationProbe probe;
    auto result = asc::Buffer::Allocate(resource, bytes, 64);
    const auto allocations = probe.count();
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    CheckStatus(test, result.status(), resource, allocations);
  }
}

template <typename Element, typename ExtentsType, typename Layout>
void CheckOwner(TestContext& test, const ExtentsType& extents, Layout layout) {
  {
    FailureResource resource;
    const asc_dense_test::AllocationProbe probe;
    auto result = asc::DenseArray<Element, ExtentsType>::Create(
        resource, extents, layout);
    const auto allocations = probe.count();
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    CheckStatus(test, result.status(), resource, allocations);
  }
  {
    FailureResource resource;
    const asc_dense_test::AllocationProbe probe;
    auto result = asc::DenseArray<Element, ExtentsType>::CreateUninitialized(
        extents, resource, layout);
    const auto allocations = probe.count();
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    CheckStatus(test, result.status(), resource, allocations);
  }
}

template <typename Element, typename Layout>
void CheckShapes(TestContext& test, Layout layout) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  const auto scalar = asc::Extents<>::Create();
  const auto empty = Shape::Create(2, 0);
  const auto matrix = Shape::Create(2, 3);
  ASC_DENSE_TEST_CHECK(test, scalar.ok() && empty.ok() && matrix.ok());
  if (!scalar.ok() || !empty.ok() || !matrix.ok()) {
    return;
  }
  CheckOwner<Element>(test, *scalar, layout);
  CheckOwner<Element>(test, *empty, layout);
  CheckOwner<Element>(test, *matrix, layout);
}

// Reading a valid prepared header isolates the allocation path from any
// parser or stream error. Only the explicit resource returns the long error.
class TextSource final : public asc::ByteSource {
 public:
  explicit TextSource(std::string_view input) : input_(input) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    if (input_.empty() || output.empty()) {
      return std::size_t{0};
    }
    output[0] = static_cast<std::byte>(input_.front());
    input_.remove_prefix(1);
    return std::size_t{1};
  }

 private:
  std::string_view input_;
};

void CheckReader(TestContext& test) {
  constexpr std::array<std::string_view, 3> kFrames{
      "ASCARRAY 1\nkind dense\nscalar f64\nrank 0\nshape\norder dim0\n"
      "count 1\ndata\n3\nend\n",
      "ASCARRAY 1\nkind dense\nscalar f64\nrank 1\nshape 0\norder dim0\n"
      "count 0\ndata\nend\n",
      "ASCARRAY 1\nkind dense\nscalar f64\nrank 1\nshape 2\norder dim0\n"
      "count 2\ndata\n3\n4\nend\n"};
  for (auto text : kFrames) {
    TextSource source(text);
    std::array<asc::extent_t, 1> metadata{};
    std::array<std::byte, 512> scratch{};
    asc::ArrayIoReport report;
    auto reader = asc::DenseArrayReader::PrepareText(
        source, metadata, scratch, asc::ArrayIoLimits{}, report);
    ASC_DENSE_TEST_CHECK(test, reader.ok());
    if (!reader.ok()) {
      continue;
    }
    FailureResource resource;
    const asc_dense_test::AllocationProbe probe;
    asc::Status status;
    if (reader->shape().empty()) {
      auto result = asc::ReadDenseArray<double, asc::Extents<>>(
          *reader, resource, asc::LayoutLeft{});
      status = asc::Status(result.status().code());
    } else {
      auto result =
          asc::ReadDenseArray<double, asc::Extents<asc::kDynamicExtent>>(
              *reader, resource, asc::LayoutRight{});
      status = asc::Status(result.status().code());
    }
    const auto allocations = probe.count();
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kAllocation);
    ASC_DENSE_TEST_EQ(test, resource.requests(), 1);
    ASC_DENSE_TEST_CHECK(test, !report.committed && !reader->ready());
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(allocations, 0));
  }
}

asc::ArrayIoLimits LargeLimits() {
  asc::ArrayIoLimits limits;
  limits.max_extent = std::numeric_limits<asc::extent_t>::max();
  limits.max_logical_elements = std::numeric_limits<asc::extent_t>::max();
  limits.max_input_bytes = std::numeric_limits<std::size_t>::max();
  limits.max_decoded_bytes = std::numeric_limits<std::size_t>::max();
  return limits;
}

template <typename T>
void CheckBoundedArithmetic(TestContext& test) {
  using asc::internal_array_io::AddSize;
  using asc::internal_array_io::MultiplySize;
  constexpr auto kMaximum = std::numeric_limits<T>::max();
  const asc_dense_test::AllocationProbe probe;
  const auto sum = AddSize(kMaximum, T{0});
  const auto product = MultiplySize(kMaximum, T{1});
  const auto zero = MultiplySize(kMaximum, T{0});
  const auto sum_overflow = AddSize(kMaximum, T{1});
  const auto product_overflow = MultiplySize(kMaximum, T{2});
  const auto allocations = probe.count();
  ASC_DENSE_TEST_CHECK(test, sum.ok() && product.ok() && zero.ok());
  if (sum.ok() && product.ok() && zero.ok()) {
    ASC_DENSE_TEST_EQ(test, *sum, kMaximum);
    ASC_DENSE_TEST_EQ(test, *product, kMaximum);
    ASC_DENSE_TEST_EQ(test, *zero, T{0});
  }
  ASC_DENSE_TEST_EQ(test, sum_overflow.status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, product_overflow.status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

void CheckBoundedConversions(TestContext& test) {
  using asc::internal_array_io::AddSize;
  using asc::internal_array_io::CastSize;
  using asc::internal_array_io::MultiplySize;
  const asc_dense_test::AllocationProbe probe;
  const auto negative_sum = AddSize(-1, 0);
  const auto negative_product = MultiplySize(0, -1);
  const auto negative_cast = CastSize<std::uint64_t>(-1);
  const auto unsigned_cast =
      CastSize<std::int64_t>(std::numeric_limits<std::uint64_t>::max());
  const auto narrow_cast = CastSize<std::uint8_t>(256);
  const auto valid_cast = CastSize<std::int8_t>(127);
  const auto allocations = probe.count();
  for (const asc::Status* status :
       {&negative_sum.status(), &negative_product.status(),
        &negative_cast.status(), &unsigned_cast.status(),
        &narrow_cast.status()}) {
    ASC_DENSE_TEST_EQ(test, status->code(), asc::ErrorCode::kOverflow);
  }
  ASC_DENSE_TEST_CHECK(test, valid_cast.ok());
  if (valid_cast.ok()) {
    ASC_DENSE_TEST_EQ(test, *valid_cast, 127);
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

// This is a 64-byte hostile header, not a descriptor claiming huge backing
// storage. Its payload length fits uint64_t but the complete frame does not.
void CheckBinaryFrameOverflow(TestContext& test) {
  std::array<std::byte, 64> frame{};
  constexpr std::string_view kMagic = "ASCARRB\n";
  for (std::size_t i = 0; i < kMagic.size(); ++i) {
    frame[i] = static_cast<std::byte>(kMagic[i]);
  }
  frame[8] = std::byte{1};
  frame[12] = std::byte{1};
  frame[13] = std::byte{10};
  frame[16] = std::byte{1};
  const auto put = [&frame](std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
      frame[offset + i] = static_cast<std::byte>(value & 255U);
      value >>= 8;
    }
  };
  constexpr auto kCount = std::numeric_limits<std::uint64_t>::max() / 8;
  put(24, kCount);
  put(40, kCount * 8);
  put(48, 64);
  put(56, kCount);
  TextSource source(std::string_view(
      reinterpret_cast<const char*>(frame.data()), frame.size()));
  std::array<asc::extent_t, 1> metadata{};
  std::array<std::byte, 512> scratch{};
  asc::ArrayIoReport report;
  const asc_dense_test::AllocationProbe probe;
  auto reader = asc::DenseArrayReader::PrepareBinary(source, metadata, scratch,
                                                     LargeLimits(), report);
  const auto allocations = probe.count();
  ASC_DENSE_TEST_CHECK(test, !reader.ok());
  ASC_DENSE_TEST_EQ(test, reader.status().code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, report.input_bytes, frame.size());
  ASC_DENSE_TEST_CHECK(test, !report.committed);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

void CheckTextProductOverflow(TestContext& test) {
  constexpr std::string_view kFrame =
      "ASCARRAY 1\nkind dense\nscalar f64\nrank 2\n"
      "shape 4294967296 4294967296\norder dim0\ncount 0\ndata\n";
  TextSource source(kFrame);
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 512> scratch{};
  asc::ArrayIoReport report;
  const asc_dense_test::AllocationProbe probe;
  auto reader = asc::DenseArrayReader::PrepareText(source, metadata, scratch,
                                                   LargeLimits(), report);
  const auto allocations = probe.count();
  ASC_DENSE_TEST_CHECK(test, !reader.ok());
  ASC_DENSE_TEST_EQ(test, reader.status().code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test, !report.committed);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

template <typename Layout>
void CheckEmptyOwnerOverflow(TestContext& test, std::string_view shape,
                             Layout layout) {
  const std::string frame =
      "ASCARRAY 1\nkind dense\nscalar f64\nrank 3\nshape " +
      std::string(shape) + "\norder dim0\ncount 0\ndata\nend\n";
  TextSource source(frame);
  std::array<asc::extent_t, 3> metadata{};
  std::array<std::byte, 512> scratch{};
  asc::ArrayIoReport report;
  auto reader = asc::DenseArrayReader::PrepareText(source, metadata, scratch,
                                                   LargeLimits(), report);
  ASC_DENSE_TEST_CHECK(test, reader.ok());
  if (!reader.ok()) {
    return;
  }
  FailureResource resource;
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent,
                             asc::kDynamicExtent>;
  const asc_dense_test::AllocationProbe probe;
  auto owner = asc::ReadDenseArray<double, Shape>(*reader, resource, layout);
  const auto allocations = probe.count();
  ASC_DENSE_TEST_CHECK(test, !owner.ok());
  ASC_DENSE_TEST_EQ(test, owner.status().code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, resource.requests(), 0);
  ASC_DENSE_TEST_CHECK(test, !report.committed && !reader->ready());
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

}  // namespace

int main() {
  TestContext test;
  CheckBuffer(test);
  CheckShapes<double>(test, asc::LayoutLeft{});
  CheckShapes<double>(test, asc::LayoutRight{});
  CheckShapes<std::complex<double>>(test, asc::LayoutLeft{});
  CheckShapes<std::complex<double>>(test, asc::LayoutRight{});
  CheckReader(test);
  CheckBoundedArithmetic<std::int8_t>(test);
  CheckBoundedArithmetic<std::uint8_t>(test);
  CheckBoundedArithmetic<std::int64_t>(test);
  CheckBoundedArithmetic<std::uint64_t>(test);
  CheckBoundedConversions(test);
  CheckBinaryFrameOverflow(test);
  CheckTextProductOverflow(test);
  CheckEmptyOwnerOverflow(test, "4294967296 4294967296 0", asc::LayoutLeft{});
  CheckEmptyOwnerOverflow(test, "4294967296 4294967296 0", asc::LayoutRight{});
  CheckEmptyOwnerOverflow(test, "0 4294967296 4294967296", asc::LayoutRight{});
  return test.Finish();
}
