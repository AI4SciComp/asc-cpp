#include <concepts>
#include <iostream>
#include <type_traits>

#include "asc/core.h"
#include "asc/dense.h"
#include "asc/expression.h"
#include "asc/random.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"
#include "asc/sparse.h"
#include "asc/utilities.h"

#if defined(ASC_M8_EXPECT_CUDA_HEADERS)
#include "asc/core/providers/cuda.h"
#include "asc/dense/providers/cuda.h"
#include "asc/random/providers/cuda.h"
#include "asc/random/providers/dense_cuda.h"
#include "asc/random/providers/sparse_cuda.h"
#include "asc/sparse/providers/cuda.h"
#endif

namespace {

using DenseExtents = asc::Extents<4, 3>;
using DenseOwner = asc::DenseArray<double, DenseExtents>;
using CoordinateOwner = asc::CoordinateArray<double, DenseExtents>;

static_assert(std::move_constructible<DenseOwner>);
static_assert(!std::copy_constructible<DenseOwner>);
static_assert(std::move_constructible<CoordinateOwner>);
static_assert(!std::copy_constructible<CoordinateOwner>);
static_assert(std::move_constructible<asc::CsrArray<double>>);
static_assert(!std::copy_constructible<asc::CsrArray<double>>);
static_assert(std::is_trivially_copyable_v<asc::DenseView<double, 2>>);
static_assert(std::is_trivially_copyable_v<asc::CoordinateView<double, 2>>);
static_assert(std::is_trivially_copyable_v<asc::CsrView<double>>);
static_assert(asc::ReadableExpression<asc::DenseView<const double, 2>>);
static_assert(asc::WritableExpression<asc::DenseView<double, 2>>);
static_assert(asc::SparseLinearAlgebraScalar<double>);

#if defined(ASC_M8_EXPECT_CUDA_HEADERS)
static_assert(!std::copy_constructible<asc::CudaMemoryResource>);
static_assert(!std::copy_constructible<asc::DenseCudaContext>);
static_assert(!std::copy_constructible<asc::SparseCudaContext>);
static_assert(std::move_constructible<asc::CudaDenseUniform01Generation>);
static_assert(std::move_constructible<asc::CudaRandomWordGeneration>);
#endif

template <typename Type>
void ReportObjectSize(const char* name) {
  std::cout << "M8_OBJECT_SIZE type=" << name << " size=" << sizeof(Type)
            << " align=" << alignof(Type) << '\n';
}

}  // namespace

int main() {
  ReportObjectSize<asc::Status>("asc::Status");
  ReportObjectSize<asc::ExecutionContext>("asc::ExecutionContext");
  ReportObjectSize<asc::DenseView<double, 2>>("asc::DenseView<double,2>");
  ReportObjectSize<asc::CoordinateView<double, 2>>(
      "asc::CoordinateView<double,2>");
  ReportObjectSize<asc::CsrView<double>>("asc::CsrView<double>");
  ReportObjectSize<asc::Philox4x32Counter>("asc::Philox4x32Counter");
#if defined(ASC_M8_EXPECT_CUDA_HEADERS)
  ReportObjectSize<asc::CudaDenseUniform01Generation>(
      "asc::CudaDenseUniform01Generation");
  ReportObjectSize<asc::CudaRandomWordGeneration>(
      "asc::CudaRandomWordGeneration");
#endif
  return 0;
}
