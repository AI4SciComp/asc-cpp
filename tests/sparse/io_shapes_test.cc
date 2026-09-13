#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/io.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;

class Source final : public asc::ByteSource {
 public:
  explicit Source(std::span<const std::byte> bytes) : bytes_(bytes) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    const auto count = std::min({bytes_.size(), output.size(), std::size_t{3}});
    if (count != 0) {
      std::copy_n(bytes_.begin(), count, output.begin());
    }
    bytes_ = bytes_.subspan(count);
    return count;
  }

 private:
  std::span<const std::byte> bytes_;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    if (input.size() > bytes.size() - size) {
      return asc::Status(asc::ErrorCode::kAllocation);
    }
    std::copy(input.begin(), input.end(), bytes.begin() + size);
    size += input.size();
    return input.size();
  }
  std::array<std::byte, 2048> bytes{};
  std::size_t size = 0;
};

template <typename T>
bool Equal(T actual, T expected, bool binary) {
  if constexpr (asc::internal_array_format::kComplex<T>) {
    return Equal(actual.real(), expected.real(), binary) &&
           Equal(actual.imag(), expected.imag(), binary);
  } else {
    if constexpr (std::is_floating_point_v<T>) {
      if (!binary && std::isnan(actual) && std::isnan(expected)) {
        return true;
      }
    }
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(actual) ==
           std::bit_cast<std::array<std::byte, sizeof(T)>>(expected);
  }
}

template <typename Owner, typename View>
void RoundTrip(TestContext& test, const View& view, bool binary) {
  using T = typename View::value_type;
  std::array<asc::extent_t, View::kRank> metadata{};
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  Sink sink;
  {
    asc_sparse_test::AllocationProbe probe;
    const auto status =
        binary
            ? asc::WriteSparseArrayBinary(view, sink, limits, scratch, report)
            : asc::WriteSparseArrayText(view, sink, limits, scratch, report);
    ASC_SPARSE_TEST_CHECK(test, status.ok());
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  Source source(std::span(sink.bytes).first(sink.size));
  asc::HostMemoryResource resource;
  auto owner =
      binary ? asc::ReadSparseArrayBinary<Owner>(source, resource, metadata,
                                                 scratch, limits, report, true)
             : asc::ReadSparseArrayText<Owner>(source, resource, metadata,
                                               scratch, limits, report, true);
  ASC_SPARSE_TEST_CHECK(test, owner.ok());
  if (!owner.ok()) {
    return;
  }
  auto loaded = owner->view();
  ASC_SPARSE_TEST_CHECK(test, loaded.ok());
  if (!loaded.ok()) {
    return;
  }
  ASC_SPARSE_TEST_CHECK(test, report.committed);
  for (asc::nnz_t i = 0; i < view.nnz(); ++i) {
    ASC_SPARSE_TEST_CHECK(test,
                          Equal(loaded->values()[i], view.values()[i], binary));
  }
  std::array<T, 2> staging{};
  Source into(std::span(sink.bytes).first(sink.size));
  {
    asc_sparse_test::AllocationProbe probe;
    const auto status = binary ? asc::ReadSparseArrayBinaryInto(
                                     into, *loaded, std::span(staging),
                                     metadata, scratch, limits, report, true)
                               : asc::ReadSparseArrayTextInto(
                                     into, *loaded, std::span(staging),
                                     metadata, scratch, limits, report, true);
    ASC_SPARSE_TEST_CHECK(test, status.ok());
    ASC_SPARSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  if (test.Finish() == 0) {
    std::cout << "sparse_shape kind="
              << static_cast<int>(
                     asc::internal_sparse_io::ViewTraits<View>::kKind)
              << " scalar="
              << static_cast<int>(asc::internal_array_io::ScalarCode<T>())
              << " rank=" << View::kRank << " nnz=" << view.nnz()
              << " binary=" << binary << '\n';
  }
}

template <typename Sequence>
struct ShapeType;
template <std::size_t... Dimensions>
struct ShapeType<std::index_sequence<Dimensions...>> {
  using Type = asc::Extents<(asc::kDynamicExtent +
                             static_cast<asc::extent_t>(0 * Dimensions))...>;
};

template <typename T, std::size_t Rank>
void Shapes(TestContext& test) {
  using Owner = asc::CoordinateArray<
      T, typename ShapeType<std::make_index_sequence<Rank>>::Type>;
  std::array<asc::extent_t, Rank> shape{};
  shape.fill(2);
  std::array<asc::index_t, 2 * Rank> coordinates{};
  std::fill(coordinates.begin() + Rank, coordinates.end(), 1);
  std::array<T, 2> values{T{0}, T{1}};
  constexpr asc::nnz_t kNonemptyCount = Rank == 0 ? 1 : 2;
  for (bool empty : {false, true}) {
    const auto count = empty ? 0 : kNonemptyCount;
    auto view = asc::CoordinateView<T, Rank>::Create(
        coordinates.data(), values.data(), shape, count,
        asc::MemorySpace::kHost);
    ASC_SPARSE_TEST_CHECK(test, view.ok());
    if (!view.ok()) {
      return;
    }
    for (bool binary : {false, true}) {
      RoundTrip<Owner>(test, *view, binary);
    }
  }
}

template <typename T>
void EmptyCompressed(TestContext& test) {
  const std::array<asc::extent_t, 2> shape{2, 3};
  const std::array<asc::nnz_t, 4> offsets{};
  auto csr = asc::CsrView<T>::Create(offsets.data(), nullptr, nullptr, shape, 0,
                                     asc::MemorySpace::kHost);
  auto csc = asc::CscView<T>::Create(offsets.data(), nullptr, nullptr, shape, 0,
                                     asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, csr.ok() && csc.ok());
  if (!csr.ok() || !csc.ok()) {
    return;
  }
  for (bool binary : {false, true}) {
    RoundTrip<asc::CsrArray<T>>(test, *csr, binary);
    RoundTrip<asc::CscArray<T>>(test, *csc, binary);
  }
}

template <typename T>
void AllShapes(TestContext& test) {
  Shapes<T, 0>(test);
  Shapes<T, 1>(test);
  Shapes<T, 3>(test);
  EmptyCompressed<T>(test);
}

template <typename T>
void Specials(TestContext& test) {
  using Real = decltype(std::real(T{}));
  using Owner = asc::CoordinateArray<T, asc::Extents<2>>;
  std::array<T, 2> values{};
  const auto nan = std::numeric_limits<Real>::quiet_NaN();
  const auto infinity = std::numeric_limits<Real>::infinity();
  if constexpr (asc::internal_array_format::kComplex<T>) {
    values = {T(infinity, -infinity), T(nan, -Real{0})};
  } else {
    values = {infinity, nan};
  }
  const std::array<asc::index_t, 2> coordinates{0, 1};
  const std::array<asc::extent_t, 1> shape{2};
  auto view = asc::CoordinateView<T, 1>::Create(
      coordinates.data(), values.data(), shape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  for (bool binary : {false, true}) {
    RoundTrip<Owner>(test, *view, binary);
  }
  values = {T{0}, T{-Real{0}}};
  for (bool binary : {false, true}) {
    RoundTrip<Owner>(test, *view, binary);
  }
}

}  // namespace

int main() {
  TestContext test;
  AllShapes<std::int8_t>(test);
  AllShapes<std::uint8_t>(test);
  AllShapes<std::int16_t>(test);
  AllShapes<std::uint16_t>(test);
  AllShapes<std::int32_t>(test);
  AllShapes<std::uint32_t>(test);
  AllShapes<std::int64_t>(test);
  AllShapes<std::uint64_t>(test);
  AllShapes<float>(test);
  AllShapes<double>(test);
  AllShapes<std::complex<float>>(test);
  AllShapes<std::complex<double>>(test);
  Specials<float>(test);
  Specials<double>(test);
  Specials<std::complex<float>>(test);
  Specials<std::complex<double>>(test);
  return test.Finish();
}
