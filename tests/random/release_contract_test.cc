#include <asc/array.h>
#include <asc/random.h>

#include <array>
#include <cstdint>
#include <limits>

int main() {
  using Extents = asc::Extents<2>;
  auto mapping = asc::LayoutLeftMapping<Extents>::Create(Extents());
  if (!mapping.ok()) {
    return 1;
  }
  std::array<double, 2> output{-3.0, -5.0};
  auto view = asc::TensorView<double, Extents>::Create(
      output.data(), mapping.value(), output.size());
  if (!view.ok()) {
    return 1;
  }

  const asc::Status status = asc::FillRandom(
      asc::ExecutionContext::Serial(), view.value(), asc::Uniform01<double>(),
      asc::RandomKey{0},
      asc::RandomCounter{0, std::numeric_limits<std::uint64_t>::max()});
  return !status.ok() && status.code() == asc::StatusCode::kOverflow &&
                 output == std::array<double, 2>{-3.0, -5.0}
             ? 0
             : 1;
}
