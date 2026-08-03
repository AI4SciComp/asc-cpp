#include "asc/random.h"

#include <array>

int main() {
  auto distribution = asc::UniformRealDistribution<double>::Create(2.0, 6.0);
  if (!distribution.ok()) {
    return 1;
  }
  asc::UniformGenerator<asc::Pcg32, double> generator(asc::Pcg32(42, 54),
                                                      *distribution);
  auto sample = generator();
  if (!sample.ok() || *sample < 2.0 || !(*sample < 6.0)) {
    return 2;
  }

  std::array<double, 2> sobol{};
  const asc::Status status = asc::GenerateSobolPoint<double>(2, sobol);
  return status.ok() && sobol == std::array<double, 2>{0.75, 0.25} ? 0 : 3;
}
