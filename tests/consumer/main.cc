#if ASC_TEST_CORE
#include <asc/core.h>
#elif ASC_TEST_UTILITIES
#include <asc/utilities.h>
#elif ASC_TEST_ARRAY
#include <asc/array.h>
#elif ASC_TEST_LINALG
#include <asc/array.h>
#include <asc/linalg.h>
#elif ASC_TEST_RANDOM
#include <asc/array.h>
#include <asc/random.h>
#elif ASC_TEST_CPP
#include <asc/cpp.h>
#endif

#include <array>
#include <string>
#include <utility>

int main() {
#if ASC_TEST_CORE
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  asc::Result<asc::MemoryResourcePtr> resource =
      context.GetMemoryResource(asc::MemorySpace::kHost);
  if (!resource.ok()) {
    return 1;
  }
  asc::Result<asc::Buffer<double>> allocation =
      asc::Buffer<double>::Allocate(4, resource.value());
  if (!allocation.ok()) {
    return 1;
  }
  asc::Buffer<double> buffer = std::move(allocation).value();
  asc::Result<double*> data = buffer.HostData();
  if (!data.ok()) {
    return 1;
  }
  for (asc::extent_t index = 0; index < buffer.GetSize(); ++index) {
    data.value()[index] = static_cast<double>(index + 1);
  }
  const asc::Buffer<double>& const_buffer = buffer;
  asc::Result<const double*> const_data = const_buffer.HostData();
  if (!const_data.ok() || const_data.value()[3] != 4.0) {
    return 1;
  }
  return context.Synchronize().ok() ? 0 : 1;
#elif ASC_TEST_UTILITIES
  asc::ConfigParser configuration;
  if (!configuration
           .TryLoadFromString("answer = 42\n",
                              asc::UnknownConfigKeyPolicy::kAdd)
           .ok()) {
    return 1;
  }
  asc::Result<std::string> serialized = configuration.Serialize();
  if (!serialized.ok() || serialized.value() != "answer = 42\n") {
    return 1;
  }

  int value = 0;
  asc::OptionParser options;
  options.AddOption<asc::Variable<int>>("n", "number", "number", 0,
                                        &value);
  const char* argv[] = {"consumer", "--number", "-7"};
  if (!options.TryParse(3, argv).ok() || value != -7) {
    return 1;
  }

  const asc::Timer timer;
  return timer.GetMeasurementCount() == 0 && timer.LastTime() == 0 &&
                 timer.TotalTime() == 0 && timer.AverageTime() == 0
             ? 0
             : 1;
#elif ASC_TEST_ARRAY
  using Extents = asc::Extents<2, asc::dynamic_extent, 2>;
  using Tensor = asc::Tensor<int, Extents>;
  asc::Result<Extents> extents = Extents::Create(3);
  if (!extents.ok()) {
    return 1;
  }
  asc::Result<Tensor> tensor = Tensor::Create(extents.value());
  if (!tensor.ok()) {
    return 1;
  }
  asc::Result<Tensor::MutableView> mutable_view = tensor.value().View();
  if (!mutable_view.ok()) {
    return 1;
  }
  mutable_view.value()(1, 2, 1) = 7;

  const Tensor& const_tensor = tensor.value();
  asc::Result<Tensor::ConstView> const_view = const_tensor.View();
  if (!const_view.ok() || const_view.value()(1, 2, 1) != 7) {
    return 1;
  }

  asc::Result<Tensor> clone = tensor.value().Clone(
      asc::ExecutionContext::Serial(), asc::MemorySpace::kHost);
  if (!clone.ok()) {
    return 1;
  }
  asc::Result<Tensor::MutableView> clone_view = clone.value().View();
  if (!clone_view.ok() ||
      clone_view.value().Data() == mutable_view.value().Data()) {
    return 1;
  }
  clone_view.value()(1, 2, 1) = 9;
  return const_view.value()(1, 2, 1) == 7 ? 0 : 1;
#elif ASC_TEST_LINALG
  using VectorExtents = asc::Extents<2>;
  using MatrixExtents = asc::Extents<2, 2>;
  auto vector_mapping =
      asc::LayoutLeftMapping<VectorExtents>::Create(VectorExtents());
  auto matrix_mapping =
      asc::LayoutLeftMapping<MatrixExtents>::Create(MatrixExtents());
  if (!vector_mapping.ok() || !matrix_mapping.ok()) {
    return 1;
  }
  std::array<double, 2> left{1.0, 2.0};
  std::array<double, 2> right{3.0, 4.0};
  std::array<double, 4> matrix{1.0, 0.0, 0.0, 2.0};
  auto left_view = asc::TensorView<const double, VectorExtents>::Create(
      left.data(), vector_mapping.value(), left.size());
  auto right_view = asc::TensorView<double, VectorExtents>::Create(
      right.data(), vector_mapping.value(), right.size());
  auto matrix_view = asc::TensorView<const double, MatrixExtents>::Create(
      matrix.data(), matrix_mapping.value(), matrix.size());
  if (!left_view.ok() || !right_view.ok() || !matrix_view.ok()) {
    return 1;
  }

  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  asc::Result<double> dot = asc::Dot(
      context, left_view.value(), right_view.value());
  if (!dot.ok() || dot.value() != 11.0) {
    return 1;
  }
  asc::Status gemv = asc::Gemv(
      context, asc::TransposeMode::kNoTranspose, 1.0,
      matrix_view.value(), left_view.value(), 0.0, right_view.value());
  return gemv.ok() && right_view.value()(0) == 1.0 &&
                 right_view.value()(1) == 4.0
             ? 0
             : 1;
#elif ASC_TEST_RANDOM
  using Extents = asc::Extents<2>;
  auto mapping = asc::LayoutRightMapping<Extents>::Create(Extents());
  if (!mapping.ok()) {
    return 1;
  }
  std::array<double, 2> output{};
  auto view = asc::TensorView<double, Extents,
                              asc::LayoutRightMapping<Extents>>::Create(
      output.data(), mapping.value(), output.size());
  if (!view.ok()) {
    return 1;
  }
  const asc::Status status = asc::FillRandom(
      asc::ExecutionContext::Serial(), view.value(), asc::Uniform01<double>(),
      asc::RandomKey{0}, asc::RandomCounter{0, 0});
  return status.ok() && output[0] == 0x1.989fa35785a70p-2 &&
                 output[1] == 0x1.f1c99948b9640p-1
             ? 0
             : 1;
#elif ASC_TEST_CPP
  return asc::Factorial(5) == 120 ? 0 : 1;
#else
  return 1;
#endif
}
