#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/cuda.h"
#include "counting_resource.h"
#include "device_test_helpers.h"
#include "test_support.h"

namespace {

using asc_dense_cuda_test::CopyAndWait;
using asc_dense_cuda_test::Data;
using asc_dense_cuda_test::TestContext;

template <typename Element>
using Vector = asc::DenseBlasVectorView<Element>;

struct BufferPair {
  asc::Buffer host;
  asc::Buffer device;
};

template <typename Element>
asc::Result<BufferPair> AllocatePair(asc::MemoryResource& pinned,
                                     asc::MemoryResource& device,
                                     std::size_t count) {
  auto host =
      asc::Buffer::Allocate(pinned, count * sizeof(Element), alignof(Element));
  if (!host.ok()) {
    return host.status();
  }
  auto device_buffer =
      asc::Buffer::Allocate(device, count * sizeof(Element), alignof(Element));
  if (!device_buffer.ok()) {
    return device_buffer.status();
  }
  return BufferPair{std::move(*host), std::move(*device_buffer)};
}

template <typename Element>
Vector<Element> MakeVector(asc::Buffer& buffer, std::size_t logical_index,
                           asc::extent_t size, asc::stride_t increment,
                           asc::MemorySpace space) {
  auto vector = Vector<Element>::Create(
      Data<Element>(buffer) + logical_index, size, increment,
      asc::ConstMemoryView(buffer.data(), buffer.size(), space));
  if (!vector.ok()) {
    std::abort();
  }
  return *vector;
}

template <typename Operation>
bool Wait(TestContext& test, Operation& operation) {
  ASC_DENSE_CUDA_CHECK(test, operation.ok());
  if (!operation.ok()) {
    std::cerr << operation.status().ToString() << '\n';
    return false;
  }
  auto query = operation->Query();
  ASC_DENSE_CUDA_CHECK(test, query.ok());
  const asc::Status status = operation->Wait();
  ASC_DENSE_CUDA_CHECK(test, status.ok());
  return status.ok();
}

template <typename Real>
void CheckNear(TestContext& test, Real actual, Real expected) {
  ASC_DENSE_CUDA_NEAR(test, actual, expected, Real{1});
}

template <typename Real>
void CheckNear(TestContext& test, std::complex<Real> actual,
               std::complex<Real> expected) {
  CheckNear(test, actual.real(), expected.real());
  CheckNear(test, actual.imag(), expected.imag());
}

template <typename Element>
void TestUpdates(TestContext& test,
                 asc_dense_cuda_test::CountingResource& pinned,
                 asc_dense_cuda_test::CountingResource& device,
                 asc::DenseCudaContext& context) {
  auto x = AllocatePair<Element>(pinned, device, 5);
  auto y = AllocatePair<Element>(pinned, device, 5);
  ASC_DENSE_CUDA_CHECK(test, x.ok());
  ASC_DENSE_CUDA_CHECK(test, y.ok());
  if (!x.ok() || !y.ok()) {
    return;
  }
  for (std::size_t index = 0; index < 5; ++index) {
    Data<Element>(x->host)[index] = static_cast<Element>(index + 1);
    Data<Element>(y->host)[index] = static_cast<Element>(10 + index);
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), x->device, x->host));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), y->device, y->host));

  auto x_vector =
      MakeVector<Element>(x->device, 4, 3, -2, asc::MemorySpace::kDevice);
  auto y_vector =
      MakeVector<Element>(y->device, 0, 3, 2, asc::MemorySpace::kDevice);
  const std::size_t allocation_checkpoint = device.allocation_calls();
  auto swapped = asc::CudaSwap(context, x_vector, y_vector);
  if (!Wait(test, swapped)) {
    return;
  }
  auto copied =
      asc::CudaCopy(context, Vector<const Element>(x_vector), y_vector);
  if (!Wait(test, copied)) {
    return;
  }
  auto scaled = asc::CudaScal(context, Element{2}, y_vector);
  if (!Wait(test, scaled)) {
    return;
  }
  auto axpy = asc::CudaAxpy(context, Element{-1},
                            Vector<const Element>(x_vector), y_vector);
  if (!Wait(test, axpy)) {
    return;
  }
  ASC_DENSE_CUDA_EQ(test, device.allocation_calls(), allocation_checkpoint);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), y->host, y->device));
  for (asc::index_t index = 0; index < 3; ++index) {
    ASC_DENSE_CUDA_EQ(
        test, Data<Element>(y->host)[static_cast<std::size_t>(2 * index)],
        static_cast<Element>(10 + 2 * index));
  }
}

template <typename Real>
// Rotation variants share buffers and an oracle table in this fixture.
// NOLINTNEXTLINE(readability-function-size)
void TestRealRotations(TestContext& test, asc::MemoryResource& pinned,
                       asc::MemoryResource& device,
                       asc::DenseCudaContext& context) {
  auto scalars = AllocatePair<Real>(pinned, device, 10);
  auto vectors = AllocatePair<Real>(pinned, device, 6);
  ASC_DENSE_CUDA_CHECK(test, scalars.ok());
  ASC_DENSE_CUDA_CHECK(test, vectors.ok());
  if (!scalars.ok() || !vectors.ok()) {
    return;
  }
  Data<Real>(scalars->host)[0] = Real{3};
  Data<Real>(scalars->host)[1] = Real{4};
  Data<Real>(scalars->host)[4] = Real{1};
  Data<Real>(scalars->host)[5] = Real{1};
  Data<Real>(scalars->host)[6] = Real{1};
  Data<Real>(scalars->host)[7] = Real{1};
  for (std::size_t index = 0; index < 6; ++index) {
    Data<Real>(vectors->host)[index] = static_cast<Real>(index + 1);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         scalars->device, scalars->host));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         vectors->device, vectors->host));
  auto a =
      MakeVector<Real>(scalars->device, 0, 1, 1, asc::MemorySpace::kDevice);
  auto b =
      MakeVector<Real>(scalars->device, 1, 1, 1, asc::MemorySpace::kDevice);
  auto c =
      MakeVector<Real>(scalars->device, 2, 1, 1, asc::MemorySpace::kDevice);
  auto s =
      MakeVector<Real>(scalars->device, 3, 1, 1, asc::MemorySpace::kDevice);
  auto rotg = asc::CudaRotg(context, a, b, c, s);
  if (!Wait(test, rotg)) {
    return;
  }

  auto x =
      MakeVector<Real>(vectors->device, 2, 3, -1, asc::MemorySpace::kDevice);
  auto y =
      MakeVector<Real>(vectors->device, 3, 3, 1, asc::MemorySpace::kDevice);
  auto rot = asc::CudaRot(context, x, y, Real{0}, Real{1});
  if (!Wait(test, rot)) {
    return;
  }

  auto d1 =
      MakeVector<Real>(scalars->device, 4, 1, 1, asc::MemorySpace::kDevice);
  auto d2 =
      MakeVector<Real>(scalars->device, 5, 1, 1, asc::MemorySpace::kDevice);
  auto x1 =
      MakeVector<Real>(scalars->device, 6, 1, 1, asc::MemorySpace::kDevice);
  auto y1 =
      MakeVector<Real>(scalars->device, 7, 1, 1, asc::MemorySpace::kDevice);
  auto parameters =
      MakeVector<Real>(scalars->device, 5, 5, 1, asc::MemorySpace::kDevice);
  auto rotmg =
      asc::CudaRotmg(context, d1, d2, x1, Vector<const Real>(y1), parameters);
  ASC_DENSE_CUDA_CHECK(test, !rotmg.ok());
  if (!rotmg.ok()) {
    ASC_DENSE_CUDA_EQ(test, rotmg.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }

  auto modified = AllocatePair<Real>(pinned, device, 11);
  ASC_DENSE_CUDA_CHECK(test, modified.ok());
  if (!modified.ok()) {
    return;
  }
  Data<Real>(modified->host)[0] = Real{1};
  Data<Real>(modified->host)[1] = Real{1};
  Data<Real>(modified->host)[2] = Real{1};
  Data<Real>(modified->host)[3] = Real{1};
  Data<Real>(modified->host)[9] = Real{1};
  Data<Real>(modified->host)[10] = Real{1};
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         modified->device, modified->host));
  d1 = MakeVector<Real>(modified->device, 0, 1, 1, asc::MemorySpace::kDevice);
  d2 = MakeVector<Real>(modified->device, 1, 1, 1, asc::MemorySpace::kDevice);
  x1 = MakeVector<Real>(modified->device, 2, 1, 1, asc::MemorySpace::kDevice);
  y1 = MakeVector<Real>(modified->device, 3, 1, 1, asc::MemorySpace::kDevice);
  parameters =
      MakeVector<Real>(modified->device, 4, 5, 1, asc::MemorySpace::kDevice);
  rotmg =
      asc::CudaRotmg(context, d1, d2, x1, Vector<const Real>(y1), parameters);
  if (!Wait(test, rotmg)) {
    return;
  }
  auto modified_x =
      MakeVector<Real>(modified->device, 9, 1, 1, asc::MemorySpace::kDevice);
  auto modified_y =
      MakeVector<Real>(modified->device, 10, 1, 1, asc::MemorySpace::kDevice);
  auto rotm = asc::CudaRotm(context, modified_x, modified_y,
                            Vector<const Real>(parameters));
  if (!Wait(test, rotm)) {
    return;
  }

  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         scalars->host, scalars->device));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         vectors->host, vectors->device));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         modified->host, modified->device));
  CheckNear(test, Data<Real>(scalars->host)[0], Real{5});
  CheckNear(test, Data<Real>(scalars->host)[2], Real{0.6});
  CheckNear(test, Data<Real>(scalars->host)[3], Real{0.8});
  CheckNear(test, Data<Real>(vectors->host)[2], Real{4});
  CheckNear(test, Data<Real>(vectors->host)[1], Real{5});
  CheckNear(test, Data<Real>(vectors->host)[0], Real{6});
  CheckNear(test, Data<Real>(vectors->host)[3], Real{-3});
  CheckNear(test, Data<Real>(vectors->host)[4], Real{-2});
  CheckNear(test, Data<Real>(vectors->host)[5], Real{-1});
  CheckNear(test, Data<Real>(modified->host)[0], Real{0.5});
  CheckNear(test, Data<Real>(modified->host)[1], Real{0.5});
  CheckNear(test, Data<Real>(modified->host)[2], Real{2});
  CheckNear(test, Data<Real>(modified->host)[9], Real{2});
  CheckNear(test, Data<Real>(modified->host)[10], Real{0});

  auto flag_zero = AllocatePair<Real>(pinned, device, 9);
  ASC_DENSE_CUDA_CHECK(test, flag_zero.ok());
  if (!flag_zero.ok()) {
    return;
  }
  Data<Real>(flag_zero->host)[0] = Real{2};
  Data<Real>(flag_zero->host)[1] = Real{1};
  Data<Real>(flag_zero->host)[2] = Real{3};
  Data<Real>(flag_zero->host)[3] = Real{1};
  for (std::size_t index = 4; index < 9; ++index) {
    Data<Real>(flag_zero->host)[index] = Real{9};
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         flag_zero->device, flag_zero->host));
  d1 = MakeVector<Real>(flag_zero->device, 0, 1, 1, asc::MemorySpace::kDevice);
  d2 = MakeVector<Real>(flag_zero->device, 1, 1, 1, asc::MemorySpace::kDevice);
  x1 = MakeVector<Real>(flag_zero->device, 2, 1, 1, asc::MemorySpace::kDevice);
  y1 = MakeVector<Real>(flag_zero->device, 3, 1, 1, asc::MemorySpace::kDevice);
  parameters =
      MakeVector<Real>(flag_zero->device, 4, 5, 1, asc::MemorySpace::kDevice);
  rotmg =
      asc::CudaRotmg(context, d1, d2, x1, Vector<const Real>(y1), parameters);
  if (!Wait(test, rotmg)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         flag_zero->host, flag_zero->device));
  CheckNear(test, Data<Real>(flag_zero->host)[0], Real{36} / Real{19});
  CheckNear(test, Data<Real>(flag_zero->host)[1], Real{18} / Real{19});
  CheckNear(test, Data<Real>(flag_zero->host)[2], Real{19} / Real{6});
  ASC_DENSE_CUDA_EQ(test, Data<Real>(flag_zero->host)[4], Real{0});
  CheckNear(test, Data<Real>(flag_zero->host)[6], Real{-1} / Real{3});
  CheckNear(test, Data<Real>(flag_zero->host)[7], Real{1} / Real{6});
}

template <typename Real>
void TestComplexOperations(TestContext& test, asc::MemoryResource& pinned,
                           asc::MemoryResource& device,
                           asc::DenseCudaContext& context) {
  using Complex = std::complex<Real>;
  auto values = AllocatePair<Complex>(pinned, device, 8);
  auto reals = AllocatePair<Real>(pinned, device, 2);
  ASC_DENSE_CUDA_CHECK(test, values.ok());
  ASC_DENSE_CUDA_CHECK(test, reals.ok());
  if (!values.ok() || !reals.ok()) {
    return;
  }
  Data<Complex>(values->host)[0] = Complex{3, 4};
  Data<Complex>(values->host)[1] = Complex{2, -1};
  Data<Complex>(values->host)[4] = Complex{1, 1};
  Data<Complex>(values->host)[5] = Complex{2, -1};
  Data<Complex>(values->host)[6] = Complex{3, -2};
  Data<Complex>(values->host)[7] = Complex{-1, 4};
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  auto a =
      MakeVector<Complex>(values->device, 0, 1, 1, asc::MemorySpace::kDevice);
  auto b =
      MakeVector<Complex>(values->device, 1, 1, 1, asc::MemorySpace::kDevice);
  auto c = MakeVector<Real>(reals->device, 0, 1, 1, asc::MemorySpace::kDevice);
  auto s =
      MakeVector<Complex>(values->device, 2, 1, 1, asc::MemorySpace::kDevice);
  auto rotg = asc::CudaRotg(context, a, Vector<const Complex>(b), c, s);
  if (!Wait(test, rotg)) {
    return;
  }
  auto x =
      MakeVector<Complex>(values->device, 4, 2, 1, asc::MemorySpace::kDevice);
  auto y =
      MakeVector<Complex>(values->device, 6, 2, 1, asc::MemorySpace::kDevice);
  auto rot = asc::CudaRot(context, x, y, Real{0}, Real{1});
  if (!Wait(test, rot)) {
    return;
  }
  auto real_scal = asc::CudaScal(context, Real{2}, x);
  if (!Wait(test, real_scal)) {
    return;
  }
  auto complex_scal = asc::CudaScal(context, Complex{0, 1}, x);
  if (!Wait(test, complex_scal)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->host, values->device));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         reals->host, reals->device));
  ASC_DENSE_CUDA_EQ(test, Data<Complex>(values->host)[1], Complex(2, -1));
  CheckNear(test, Data<Real>(reals->host)[0], Real{5} / std::sqrt(Real{30}));
  CheckNear(test, Data<Complex>(values->host)[4], Complex{4, 6});
  CheckNear(test, Data<Complex>(values->host)[5], Complex{-8, -2});
}

template <typename Real>
void TestRealReductions(TestContext& test, asc::MemoryResource& pinned,
                        asc::MemoryResource& device,
                        asc::DenseCudaContext& context) {
  auto values = AllocatePair<Real>(pinned, device, 5);
  auto result = AllocatePair<Real>(pinned, device, 1);
  auto index = AllocatePair<asc::index_t>(pinned, device, 2);
  ASC_DENSE_CUDA_CHECK(test, values.ok());
  ASC_DENSE_CUDA_CHECK(test, result.ok());
  ASC_DENSE_CUDA_CHECK(test, index.ok());
  if (!values.ok() || !result.ok() || !index.ok()) {
    return;
  }
  for (std::size_t position = 0; position < 5; ++position) {
    Data<Real>(values->host)[position] = static_cast<Real>(position + 1);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  auto left =
      MakeVector<Real>(values->device, 4, 3, -2, asc::MemorySpace::kDevice);
  auto right =
      MakeVector<Real>(values->device, 0, 3, 2, asc::MemorySpace::kDevice);
  auto scalar =
      MakeVector<Real>(result->device, 0, 1, 1, asc::MemorySpace::kDevice);
  auto dot = asc::CudaDot(context, Vector<const Real>(left),
                          Vector<const Real>(right), scalar);
  if (!Wait(test, dot)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         result->host, result->device));
  ASC_DENSE_CUDA_EQ(test, Data<Real>(result->host)[0], Real{19});

  auto nrm2 = asc::CudaNrm2(context, Vector<const Real>(left), scalar);
  if (!Wait(test, nrm2)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         result->host, result->device));
  CheckNear(test, Data<Real>(result->host)[0], std::sqrt(Real{35}));
  auto asum = asc::CudaAsum(context, Vector<const Real>(left), scalar);
  if (!Wait(test, asum)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         result->host, result->device));
  ASC_DENSE_CUDA_EQ(test, Data<Real>(result->host)[0], Real{9});
  auto public_index = MakeVector<asc::index_t>(index->device, 0, 1, 1,
                                               asc::MemorySpace::kDevice);
  asc::MutableMemoryView workspace(Data<asc::index_t>(index->device) + 1,
                                   sizeof(asc::index_t),
                                   asc::MemorySpace::kDevice);
  auto iamax = asc::CudaIamax(context, Vector<const Real>(right), public_index,
                              workspace);
  if (!Wait(test, iamax)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         index->host, index->device));
  ASC_DENSE_CUDA_EQ(test, Data<asc::index_t>(index->host)[0], asc::index_t{2});

  Data<Real>(values->host)[0] = Real{5};
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  iamax = asc::CudaIamax(context, Vector<const Real>(left), public_index,
                         workspace);
  if (!Wait(test, iamax)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         index->host, index->device));
  ASC_DENSE_CUDA_EQ(test, Data<asc::index_t>(index->host)[0], asc::index_t{0});
}

void TestMixedDots(TestContext& test, asc::MemoryResource& pinned,
                   asc::MemoryResource& device,
                   asc::DenseCudaContext& context) {
  auto values = AllocatePair<float>(pinned, device, 6);
  auto float_result = AllocatePair<float>(pinned, device, 1);
  auto double_result = AllocatePair<double>(pinned, device, 1);
  ASC_DENSE_CUDA_CHECK(test, values.ok());
  ASC_DENSE_CUDA_CHECK(test, float_result.ok());
  ASC_DENSE_CUDA_CHECK(test, double_result.ok());
  if (!values.ok() || !float_result.ok() || !double_result.ok()) {
    return;
  }
  for (std::size_t index = 0; index < 6; ++index) {
    Data<float>(values->host)[index] = static_cast<float>(index + 1);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  auto left =
      MakeVector<float>(values->device, 0, 3, 1, asc::MemorySpace::kDevice);
  auto right =
      MakeVector<float>(values->device, 3, 3, 1, asc::MemorySpace::kDevice);
  auto single = MakeVector<float>(float_result->device, 0, 1, 1,
                                  asc::MemorySpace::kDevice);
  auto extended = MakeVector<double>(double_result->device, 0, 1, 1,
                                     asc::MemorySpace::kDevice);
  auto sdsdot = asc::CudaDot(context, 2.0F, Vector<const float>(left),
                             Vector<const float>(right), single);
  if (!Wait(test, sdsdot)) {
    return;
  }
  auto dsdot = asc::CudaDot(context, asc::DenseBlasDotAccumulation::kDouble,
                            Vector<const float>(left),
                            Vector<const float>(right), extended);
  if (!Wait(test, dsdot)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), float_result->host,
                        float_result->device));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), double_result->host,
                        double_result->device));
  ASC_DENSE_CUDA_EQ(test, Data<float>(float_result->host)[0], 34.0F);
  ASC_DENSE_CUDA_EQ(test, Data<double>(double_result->host)[0], 32.0);

  Data<float>(values->host)[0] = 1.0e8F;
  Data<float>(values->host)[1] = 1.0F;
  Data<float>(values->host)[2] = -1.0e8F;
  Data<float>(values->host)[3] = 1.0F;
  Data<float>(values->host)[4] = 1.0F;
  Data<float>(values->host)[5] = 1.0F;
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  sdsdot = asc::CudaDot(context, 0.0F, Vector<const float>(left),
                        Vector<const float>(right), single);
  if (!Wait(test, sdsdot)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), float_result->host,
                        float_result->device));
  ASC_DENSE_CUDA_EQ(test, Data<float>(float_result->host)[0], 1.0F);
}

template <typename Real>
// Complex reduction variants share buffers and edge-case expectations.
// NOLINTNEXTLINE(readability-function-size)
void TestComplexReductions(TestContext& test, asc::MemoryResource& pinned,
                           asc::MemoryResource& device,
                           asc::DenseCudaContext& context) {
  using Complex = std::complex<Real>;
  auto values = AllocatePair<Complex>(pinned, device, 4);
  auto complex_result = AllocatePair<Complex>(pinned, device, 1);
  auto real_result = AllocatePair<Real>(pinned, device, 1);
  auto index = AllocatePair<asc::index_t>(pinned, device, 2);
  ASC_DENSE_CUDA_CHECK(test, values.ok());
  ASC_DENSE_CUDA_CHECK(test, complex_result.ok());
  ASC_DENSE_CUDA_CHECK(test, real_result.ok());
  ASC_DENSE_CUDA_CHECK(test, index.ok());
  if (!values.ok() || !complex_result.ok() || !real_result.ok() ||
      !index.ok()) {
    return;
  }
  Data<Complex>(values->host)[0] = Complex{1, 2};
  Data<Complex>(values->host)[1] = Complex{3, -1};
  Data<Complex>(values->host)[2] = Complex{2, -1};
  Data<Complex>(values->host)[3] = Complex{-2, 4};
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  auto left =
      MakeVector<Complex>(values->device, 0, 2, 1, asc::MemorySpace::kDevice);
  auto right =
      MakeVector<Complex>(values->device, 2, 2, 1, asc::MemorySpace::kDevice);
  auto complex_scalar = MakeVector<Complex>(complex_result->device, 0, 1, 1,
                                            asc::MemorySpace::kDevice);
  auto real_scalar =
      MakeVector<Real>(real_result->device, 0, 1, 1, asc::MemorySpace::kDevice);
  auto dotu = asc::CudaDotu(context, Vector<const Complex>(left),
                            Vector<const Complex>(right), complex_scalar);
  if (!Wait(test, dotu)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), complex_result->host,
                        complex_result->device));
  CheckNear(
      test, Data<Complex>(complex_result->host)[0],
      Data<Complex>(values->host)[0] * Data<Complex>(values->host)[2] +
          Data<Complex>(values->host)[1] * Data<Complex>(values->host)[3]);
  auto dotc = asc::CudaDotc(context, Vector<const Complex>(left),
                            Vector<const Complex>(right), complex_scalar);
  if (!Wait(test, dotc)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), complex_result->host,
                        complex_result->device));
  CheckNear(test, Data<Complex>(complex_result->host)[0],
            std::conj(Data<Complex>(values->host)[0]) *
                    Data<Complex>(values->host)[2] +
                std::conj(Data<Complex>(values->host)[1]) *
                    Data<Complex>(values->host)[3]);
  auto nrm2 = asc::CudaNrm2(context, Vector<const Complex>(left), real_scalar);
  if (!Wait(test, nrm2)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), real_result->host,
                        real_result->device));
  CheckNear(test, Data<Real>(real_result->host)[0], std::sqrt(Real{15}));
  auto asum = asc::CudaAsum(context, Vector<const Complex>(left), real_scalar);
  if (!Wait(test, asum)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), real_result->host,
                        real_result->device));
  ASC_DENSE_CUDA_EQ(test, Data<Real>(real_result->host)[0], Real{7});
  auto public_index = MakeVector<asc::index_t>(index->device, 0, 1, 1,
                                               asc::MemorySpace::kDevice);
  asc::MutableMemoryView workspace(Data<asc::index_t>(index->device) + 1,
                                   sizeof(asc::index_t),
                                   asc::MemorySpace::kDevice);
  auto iamax = asc::CudaIamax(context, Vector<const Complex>(left),
                              public_index, workspace);
  if (!Wait(test, iamax)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         index->host, index->device));
  ASC_DENSE_CUDA_EQ(test, Data<asc::index_t>(index->host)[0], asc::index_t{1});

  Data<Complex>(values->host)[0] = Complex{2, 1};
  Data<Complex>(values->host)[1] = Complex{1, 2};
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  auto reverse_left =
      MakeVector<Complex>(values->device, 1, 2, -1, asc::MemorySpace::kDevice);
  iamax = asc::CudaIamax(context, Vector<const Complex>(reverse_left),
                         public_index, workspace);
  if (!Wait(test, iamax)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         index->host, index->device));
  ASC_DENSE_CUDA_EQ(test, Data<asc::index_t>(index->host)[0], asc::index_t{0});
}

template <typename Real>
void TestExtremeNrm2(TestContext& test, asc::MemoryResource& pinned,
                     asc::MemoryResource& device,
                     asc::DenseCudaContext& context) {
  auto values = AllocatePair<Real>(pinned, device, 2);
  auto result = AllocatePair<Real>(pinned, device, 1);
  ASC_DENSE_CUDA_CHECK(test, values.ok());
  ASC_DENSE_CUDA_CHECK(test, result.ok());
  if (!values.ok() || !result.ok()) {
    return;
  }
  const Real large = std::numeric_limits<Real>::max() / Real{4};
  Data<Real>(values->host)[0] = large;
  Data<Real>(values->host)[1] = large;
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  auto operand =
      MakeVector<Real>(values->device, 0, 2, 1, asc::MemorySpace::kDevice);
  auto scalar =
      MakeVector<Real>(result->device, 0, 1, 1, asc::MemorySpace::kDevice);
  auto operation = asc::CudaNrm2(context, Vector<const Real>(operand), scalar);
  if (!Wait(test, operation)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         result->host, result->device));
  ASC_DENSE_CUDA_CHECK(test, std::isfinite(Data<Real>(result->host)[0]));
  CheckNear(test, Data<Real>(result->host)[0], std::hypot(large, large));
}

// Invalid descriptors share a known-good baseline and allocation checkpoints.
// NOLINTNEXTLINE(readability-function-size)
void TestEmptyAndValidation(TestContext& test, asc::MemoryResource& pinned,
                            asc::MemoryResource& device,
                            asc::DenseCudaContext& context) {
  auto result = AllocatePair<asc::index_t>(pinned, device, 2);
  auto scalar_result = AllocatePair<float>(pinned, device, 1);
  auto double_result = AllocatePair<double>(pinned, device, 1);
  auto values = AllocatePair<float>(pinned, device, 4);
  ASC_DENSE_CUDA_CHECK(test, result.ok());
  ASC_DENSE_CUDA_CHECK(test, scalar_result.ok());
  ASC_DENSE_CUDA_CHECK(test, double_result.ok());
  ASC_DENSE_CUDA_CHECK(test, values.ok());
  if (!result.ok() || !scalar_result.ok() || !double_result.ok() ||
      !values.ok()) {
    return;
  }
  auto empty = Vector<float>::Create(
      nullptr, 0, -1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  ASC_DENSE_CUDA_CHECK(test, empty.ok());
  if (!empty.ok()) {
    return;
  }
  const Vector<const float> const_empty(*empty);
  auto scalar = MakeVector<float>(scalar_result->device, 0, 1, 1,
                                  asc::MemorySpace::kDevice);
  auto double_scalar = MakeVector<double>(double_result->device, 0, 1, 1,
                                          asc::MemorySpace::kDevice);
  using Accumulation = asc::DenseBlasDotAccumulation;
  // Deliberately invalid to test boundary validation.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid_accumulation = static_cast<Accumulation>(99);
  auto invalid_dot = asc::CudaDot(context, invalid_accumulation, const_empty,
                                  const_empty, double_scalar);
  ASC_DENSE_CUDA_CHECK(test, !invalid_dot.ok());
  if (!invalid_dot.ok()) {
    ASC_DENSE_CUDA_EQ(test, invalid_dot.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  auto empty_dot = asc::CudaDot(context, const_empty, const_empty, scalar);
  if (!Wait(test, empty_dot)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), scalar_result->host,
                        scalar_result->device));
  ASC_DENSE_CUDA_EQ(test, Data<float>(scalar_result->host)[0], 0.0F);
  auto empty_nrm2 = asc::CudaNrm2(context, const_empty, scalar);
  if (!Wait(test, empty_nrm2)) {
    return;
  }
  auto empty_asum = asc::CudaAsum(context, const_empty, scalar);
  if (!Wait(test, empty_asum)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), scalar_result->host,
                        scalar_result->device));
  ASC_DENSE_CUDA_EQ(test, Data<float>(scalar_result->host)[0], 0.0F);
  auto empty_sdsdot =
      asc::CudaDot(context, 2.0F, const_empty, const_empty, scalar);
  if (!Wait(test, empty_sdsdot)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), scalar_result->host,
                        scalar_result->device));
  ASC_DENSE_CUDA_EQ(test, Data<float>(scalar_result->host)[0], 2.0F);
  auto empty_dsdot =
      asc::CudaDot(context, asc::DenseBlasDotAccumulation::kDouble, const_empty,
                   const_empty, double_scalar);
  if (!Wait(test, empty_dsdot)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), double_result->host,
                        double_result->device));
  ASC_DENSE_CUDA_EQ(test, Data<double>(double_result->host)[0], 0.0);

  auto public_index = MakeVector<asc::index_t>(result->device, 0, 1, 1,
                                               asc::MemorySpace::kDevice);
  asc::MutableMemoryView workspace(Data<asc::index_t>(result->device) + 1,
                                   sizeof(asc::index_t),
                                   asc::MemorySpace::kDevice);
  auto iamax = asc::CudaIamax(context, Vector<const float>(*empty),
                              public_index, workspace);
  if (!Wait(test, iamax)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         result->host, result->device));
  ASC_DENSE_CUDA_EQ(test, Data<asc::index_t>(result->host)[0],
                    asc::index_t{-1});

  asc::MutableMemoryView host_workspace(Data<asc::index_t>(result->host) + 1,
                                        sizeof(asc::index_t),
                                        asc::MemorySpace::kPinnedHost);
  auto rejected = asc::CudaIamax(context, Vector<const float>(*empty),
                                 public_index, host_workspace);
  ASC_DENSE_CUDA_CHECK(test, !rejected.ok());
  if (!rejected.ok()) {
    ASC_DENSE_CUDA_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kMemoryAccess);
  }

  asc::MutableMemoryView short_workspace(Data<asc::index_t>(result->device) + 1,
                                         0, asc::MemorySpace::kDevice);
  rejected =
      asc::CudaIamax(context, const_empty, public_index, short_workspace);
  ASC_DENSE_CUDA_CHECK(test, !rejected.ok());
  if (!rejected.ok()) {
    ASC_DENSE_CUDA_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  asc::MutableMemoryView overlapping_workspace(
      Data<asc::index_t>(result->device), sizeof(asc::index_t),
      asc::MemorySpace::kDevice);
  rejected =
      asc::CudaIamax(context, const_empty, public_index, overlapping_workspace);
  ASC_DENSE_CUDA_CHECK(test, !rejected.ok());
  if (!rejected.ok()) {
    ASC_DENSE_CUDA_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }

  Data<float>(values->host)[0] = std::numeric_limits<float>::quiet_NaN();
  Data<float>(values->host)[1] = 7.0F;
  Data<float>(values->host)[2] = 2.0F;
  Data<float>(values->host)[3] = 3.0F;
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->device, values->host));
  auto nan_source =
      MakeVector<float>(values->device, 0, 1, 1, asc::MemorySpace::kDevice);
  auto destination =
      MakeVector<float>(values->device, 1, 1, 1, asc::MemorySpace::kDevice);
  auto zero_axpy = asc::CudaAxpy(context, 0.0F, Vector<const float>(nan_source),
                                 destination);
  if (!Wait(test, zero_axpy)) {
    return;
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         values->host, values->device));
  ASC_DENSE_CUDA_EQ(test, Data<float>(values->host)[1], 7.0F);

  auto source =
      MakeVector<float>(values->device, 0, 3, 1, asc::MemorySpace::kDevice);
  auto partial =
      MakeVector<float>(values->device, 1, 3, 1, asc::MemorySpace::kDevice);
  auto overlap_copy =
      asc::CudaCopy(context, Vector<const float>(source), partial);
  ASC_DENSE_CUDA_CHECK(test, !overlap_copy.ok());
  if (!overlap_copy.ok()) {
    ASC_DENSE_CUDA_EQ(test, overlap_copy.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  auto overlap_reduction =
      asc::CudaNrm2(context, Vector<const float>(source), nan_source);
  ASC_DENSE_CUDA_CHECK(test, !overlap_reduction.ok());
  if (!overlap_reduction.ok()) {
    ASC_DENSE_CUDA_EQ(test, overlap_reduction.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
}

}  // namespace

int main() {
  if (asc_dense_cuda_test::ForceNoCudaDevice()) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  TestContext test;
  auto count = asc::CudaDeviceCount();
  ASC_DENSE_CUDA_CHECK(test, count.ok());
  if (!count.ok()) {
    return test.Finish();
  }
  if (*count == 0) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  auto pinned_upstream =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device_upstream =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto execution = asc::CreateCudaExecutionContext(0);
  ASC_DENSE_CUDA_CHECK(test, pinned_upstream.ok());
  ASC_DENSE_CUDA_CHECK(test, device_upstream.ok());
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  if (!pinned_upstream.ok() || !device_upstream.ok() || !execution.ok()) {
    return test.Finish();
  }
  auto context = asc::DenseCudaContext::Create(*execution);
  ASC_DENSE_CUDA_CHECK(test, context.ok());
  if (!context.ok()) {
    return test.Finish();
  }
  asc_dense_cuda_test::CountingResource pinned(**pinned_upstream);
  asc_dense_cuda_test::CountingResource device(**device_upstream);

  TestUpdates<float>(test, pinned, device, *context);
  TestUpdates<double>(test, pinned, device, *context);
  TestUpdates<std::complex<float>>(test, pinned, device, *context);
  TestUpdates<std::complex<double>>(test, pinned, device, *context);
  TestRealRotations<float>(test, pinned, device, *context);
  TestRealRotations<double>(test, pinned, device, *context);
  TestComplexOperations<float>(test, pinned, device, *context);
  TestComplexOperations<double>(test, pinned, device, *context);
  TestRealReductions<float>(test, pinned, device, *context);
  TestRealReductions<double>(test, pinned, device, *context);
  TestMixedDots(test, pinned, device, *context);
  TestComplexReductions<float>(test, pinned, device, *context);
  TestComplexReductions<double>(test, pinned, device, *context);
  TestExtremeNrm2<float>(test, pinned, device, *context);
  TestExtremeNrm2<double>(test, pinned, device, *context);
  TestEmptyAndValidation(test, pinned, device, *context);

  ASC_DENSE_CUDA_EQ(test, pinned.live_allocations(), std::size_t{0});
  ASC_DENSE_CUDA_EQ(test, device.live_allocations(), std::size_t{0});
  return test.Finish();
}
