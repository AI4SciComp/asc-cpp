#include <asc/array.h>
#include <asc/random.h>

#include <array>

double RandomOdrB() {
  using Extents = asc::Extents<2>;
  auto mapping = asc::LayoutRightMapping<Extents>::Create(Extents());
  if (!mapping.ok()) {
    return -1.0;
  }
  std::array<double, 2> output{};
  auto view = asc::TensorView<double, Extents,
                              asc::LayoutRightMapping<Extents>>::Create(
      output.data(), mapping.value(), output.size());
  if (!view.ok()) {
    return -1.0;
  }
  const asc::Status status = asc::FillRandom(
      asc::ExecutionContext::Serial(), view.value(), asc::Uniform01<double>(),
      asc::RandomKey{0}, asc::RandomCounter{0, 0});
  return status.ok() ? output[0] : -1.0;
}
