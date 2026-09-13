#include <array>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "../../src/dense/lapack/internal_lu_band_expert_counts.h"
#include "../dense/test_support.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack_general_band.h"
namespace {
namespace counts = asc::internal_lu_band_expert_counts;
using asc_dense_test::TestContext;
void Bounds(TestContext& test, asc::extent_t maximum) {
  ASC_DENSE_TEST_CHECK(
      test, counts::CompactStorage(maximum, 0, 0, maximum - 1, maximum, maximum)
                .ok());
  ASC_DENSE_TEST_EQ(
      test, counts::CompactStorage(0, 0, 0, maximum, maximum, maximum).code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test,
                       counts::Equilibrate(maximum, 0, 0, 0, 1, maximum).ok());
  ASC_DENSE_TEST_EQ(test,
                    counts::Equilibrate(maximum, 1, 0, 0, 1, maximum).code(),
                    asc::ErrorCode::kOverflow);
  const auto half = maximum / 2;
  ASC_DENSE_TEST_CHECK(
      test, counts::Equilibrate(half, maximum - half, 0, 0, 1, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test,
      counts::Equilibrate(half + 1, maximum - half, 0, 0, 1, maximum).code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(
      test,
      counts::Equilibrate(10, 10, 0, maximum - 11, maximum - 10, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test,
      counts::Equilibrate(10, 10, 0, maximum - 10, maximum - 9, maximum).code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test,
                       counts::Condition(maximum / 3, 0, 0, 1, maximum).ok());
  ASC_DENSE_TEST_EQ(test,
                    counts::Condition(maximum / 3 + 1, 0, 0, 1, maximum).code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(
      test, counts::Condition(2, 0, maximum - 1, maximum, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test, counts::Condition(3, 0, maximum - 1, maximum, maximum).code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test, counts::Refine(1, 0, maximum - 2, maximum - 1,
                                            maximum - 1, 1, 1, 1, maximum)
                                 .ok());
  ASC_DENSE_TEST_EQ(
      test,
      counts::Refine(1, 0, maximum - 1, maximum, maximum, 1, 1, 1, maximum)
          .code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(
      test, counts::Refine(0, 0, 0, 1, 1, maximum - 1, 1, 1, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test, counts::Refine(0, 0, 0, 1, 1, maximum, 1, 1, maximum).code(),
      asc::ErrorCode::kOverflow);
  for (const int mode : {0, 1, 2}) {
    ASC_DENSE_TEST_CHECK(
        test, counts::Driver(1, 0, maximum - 2, maximum - 1, maximum - 1, 0, 1,
                             1, mode, maximum)
                  .ok());
    ASC_DENSE_TEST_EQ(test,
                      counts::Driver(1, 0, maximum - 1, maximum, maximum, 0, 1,
                                     1, mode, maximum)
                          .code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         counts::Driver(maximum / 3, 0, 0, 1, 1, 0, maximum / 3,
                                        maximum / 3, mode, maximum)
                             .ok());
    ASC_DENSE_TEST_EQ(
        test,
        counts::Driver(maximum / 3 + 1, 0, 0, 1, 1, 0, maximum / 3 + 1,
                       maximum / 3 + 1, mode, maximum)
            .code(),
        asc::ErrorCode::kOverflow);
  }
}
template <typename T>
void Descriptor(TestContext& test) {
  std::array<T, 32> storage{};
  using View = asc::ReferenceGeneralBandView<T>;
  const asc::ConstMemoryView backing{storage.data(), sizeof(storage),
                                     asc::MemorySpace::kHost};
  const auto wide = View::Create(storage.data() + 1, 1, 1, 8, 7, 16, backing);
  ASC_DENSE_TEST_CHECK(test, wide.ok());
  if (wide.ok()) {
    ASC_DENSE_TEST_EQ(test, wide->diagonal_row(), 7);
    ASC_DENSE_TEST_EQ(test, wide->storage().rows(), 16);
  }
  const auto empty_rows =
      View::Create(storage.data() + 1, 0, 3, 2, 4, 9, backing);
  ASC_DENSE_TEST_CHECK(test, empty_rows.ok());
  const asc::ConstMemoryView short_backing{storage.data() + 1, 26 * sizeof(T),
                                           asc::MemorySpace::kHost};
  ASC_DENSE_TEST_CHECK(
      test,
      !View::Create(storage.data() + 1, 0, 3, 2, 4, 9, short_backing).ok());
  ASC_DENSE_TEST_CHECK(
      test, View::Create(nullptr, std::numeric_limits<asc::extent_t>::max(), 0,
                         2, 3, 6, {nullptr, 0, asc::MemorySpace::kHost})
                .ok());
  ASC_DENSE_TEST_EQ(
      test,
      View::Create(storage.data(), 1, 1, 8, 7, 15, backing).status().code(),
      asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(
      test,
      View::Create(storage.data(), 1, 1, -1, 0, 1, backing).status().code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(
      test,
      View::Create(storage.data(), 1, 1,
                   std::numeric_limits<asc::extent_t>::max(), 1, 1, backing)
          .status()
          .code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(
      test,
      View::Create(storage.data(), 1, std::numeric_limits<asc::extent_t>::max(),
                   0, 0, 16, backing)
          .status()
          .code(),
      asc::ErrorCode::kOverflow);
}
void PivotDescriptor(TestContext& test) {
  std::array<asc::index_t, 5> raw{0, -1, 7, 3, 99};
  using Vector = asc::DenseBlasVectorView<const asc::index_t>;
  const auto vector = Vector::Create(
      raw.data(), 5, 1, {raw.data(), sizeof(raw), asc::MemorySpace::kHost});
  ASC_DENSE_TEST_CHECK(test, vector.ok());
  const auto descriptor = asc::ReferenceLuBandPivotView::Create(*vector);
  ASC_DENSE_TEST_CHECK(test, descriptor.ok());
  ASC_DENSE_TEST_EQ(test, descriptor->values()[1], -1);
  ASC_DENSE_TEST_EQ(test, descriptor->values()[4], 99);
  ASC_DENSE_TEST_EQ(test, descriptor->storage().data(), raw.data());
  const auto strided = Vector::Create(
      raw.data(), 2, 2, {raw.data(), sizeof(raw), asc::MemorySpace::kHost});
  ASC_DENSE_TEST_CHECK(test, strided.ok());
  ASC_DENSE_TEST_EQ(
      test, asc::ReferenceLuBandPivotView::Create(*strided).status().code(),
      asc::ErrorCode::kShape);
  const auto device = Vector::Create(
      raw.data(), 5, 1, {raw.data(), sizeof(raw), asc::MemorySpace::kDevice});
  ASC_DENSE_TEST_CHECK(test, device.ok());
  ASC_DENSE_TEST_EQ(
      test, asc::ReferenceLuBandPivotView::Create(*device).status().code(),
      asc::ErrorCode::kMemoryAccess);
}
}  // namespace
int main() {
  TestContext test;
  PivotDescriptor(test);
  for (const asc::extent_t maximum :
       {asc::extent_t{127},
        static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    Bounds(test, maximum);
  }
  Descriptor<float>(test);
  Descriptor<double>(test);
  Descriptor<std::complex<float>>(test);
  Descriptor<std::complex<double>>(test);
  std::printf(
      "Pure source-expression boundaries at signed 7/31/63-bit maxima; "
      "all-scalar compact full-backing/empty/bandwidth factories\n");
  return test.Finish();
}
